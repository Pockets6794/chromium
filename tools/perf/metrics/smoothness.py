# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric
from metrics import rendering_stats
from metrics import statistics
from telemetry.page import page_measurement

TIMELINE_MARKER = 'Smoothness'


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)

class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NotEnoughFramesError, self).__init__(
        'Page output less than two frames')


class NoSupportedActionError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NoSupportedActionError, self).__init__(
        'None of the actions is supported by smoothness measurement')


def _GetSyntheticDelayCategoriesFromPage(page):
  if not hasattr(page, 'synthetic_delays'):
    return []
  result = []
  for delay, options in page.synthetic_delays.items():
    options = '%f;%s' % (options.get('target_duration', 0),
                         options.get('mode', 'static'))
    result.append('DELAY(%s;%s)' % (delay, options))
  return result


class SmoothnessMetric(Metric):
  def __init__(self):
    super(SmoothnessMetric, self).__init__()
    self._stats = None
    self._actions = []

  def AddActionToIncludeInMetric(self, action):
    self._actions.append(action)

  def Start(self, page, tab):
    custom_categories = ['webkit.console', 'benchmark']
    custom_categories += _GetSyntheticDelayCategoriesFromPage(page)
    tab.browser.StartTracing(','.join(custom_categories), 60)
    tab.ExecuteJavaScript('console.time("' + TIMELINE_MARKER + '")')
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def Stop(self, page, tab):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    tab.ExecuteJavaScript('console.timeEnd("' + TIMELINE_MARKER + '")')
    timeline_model = tab.browser.StopTracing().AsTimelineModel()
    timeline_ranges = [ action.GetActiveRangeOnTimeline(timeline_model)
                        for action in self._actions ]

    renderer_process = timeline_model.GetRendererProcessFromTab(tab)
    self._stats = rendering_stats.RenderingStats(
        renderer_process, timeline_ranges)

    if not self._stats.frame_times:
      raise NotEnoughFramesError()

  def SetStats(self, stats):
    """ Pass in a RenderingStats object directly. For unittests that don't call
        Start/Stop.
    """
    self._stats = stats

  def AddResults(self, tab, results):
    # List of raw frame times.
    results.Add('frame_times', 'ms', self._stats.frame_times)

    # Arithmetic mean of frame times.
    mean_frame_time = statistics.ArithmeticMean(self._stats.frame_times,
                                                len(self._stats.frame_times))
    results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

    # Absolute discrepancy of frame time stamps.
    jank = statistics.FrameDiscrepancy(self._stats.frame_timestamps)
    results.Add('jank', '', round(jank, 4))

    # Are we hitting 60 fps for 95 percent of all frames?
    # We use 19ms as a somewhat looser threshold, instead of 1000.0/60.0.
    percentile_95 = statistics.Percentile(self._stats.frame_times, 95.0)
    results.Add('mostly_smooth', 'score', 1.0 if percentile_95 < 19.0 else 0.0)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
