// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/media_stream_video_track.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_frame.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MediaStreamVideoTrack_0_1>() {
  return PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1;
}

}  // namespace

MediaStreamVideoTrack::MediaStreamVideoTrack() {
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    const MediaStreamVideoTrack& other) : Resource(other) {
}

MediaStreamVideoTrack::MediaStreamVideoTrack(const Resource& resource)
    : Resource(resource) {
  PP_DCHECK(IsMediaStreamVideoTrack(resource));
}

MediaStreamVideoTrack::MediaStreamVideoTrack(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
}

int32_t MediaStreamVideoTrack::Configure(uint32_t frame_buffer_size) {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->Configure(
        pp_resource(), frame_buffer_size);
  }
  return PP_ERROR_NOINTERFACE;
}

std::string MediaStreamVideoTrack::GetId() const {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    pp::Var id(PASS_REF, get_interface<PPB_MediaStreamVideoTrack_0_1>()->GetId(
        pp_resource()));
    if (id.is_string())
      return id.AsString();
  }
  return std::string();
}

bool MediaStreamVideoTrack::HasEnded() const {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_0_1>()->HasEnded(
        pp_resource()));
  }
  return true;
}

int32_t MediaStreamVideoTrack::GetFrame(
    const CompletionCallbackWithOutput<VideoFrame>& cc) {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->GetFrame(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t MediaStreamVideoTrack::RecycleFrame(const VideoFrame& frame) {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->RecycleFrame(
        pp_resource(), frame.pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

void MediaStreamVideoTrack::Close() {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>())
    get_interface<PPB_MediaStreamVideoTrack_0_1>()->Close(pp_resource());
}

bool MediaStreamVideoTrack::IsMediaStreamVideoTrack(const Resource& resource) {
  if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_0_1>()->
        IsMediaStreamVideoTrack(resource.pp_resource()));
  }
  return false;
}

}  // namespace pp
