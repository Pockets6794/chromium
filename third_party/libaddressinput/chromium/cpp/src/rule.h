// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// An object to store validation rules.

#ifndef I18N_ADDRESSINPUT_RULE_H_
#define I18N_ADDRESSINPUT_RULE_H_

#include <libaddressinput/address_field.h>
#include <libaddressinput/util/basictypes.h>

#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

// Stores the validation rules. Sample usage:
//    Rule rule;
//    if (rule.ParseSerializedRule("{\"fmt\": \"%A%n%C%S %Z\"}")) {
//      Process(rule.GetFormat());
//    }
class Rule {
 public:
  Rule();
  ~Rule();

  // Returns the default rule at a country level. If a country does not specify
  // address format, for example, then the format from this rule should be used
  // instead.
  static const Rule& GetDefault();

  // Copies all data from |rule|.
  void CopyFrom(const Rule& rule);

  // Parses |serialized_rule|. Returns |true| if the |serialized_rule| has valid
  // format (JSON dictionary).
  bool ParseSerializedRule(const std::string& serialized_rule);

  // Returns the address format for this rule.
  const std::vector<std::vector<AddressField> >& GetFormat() const {
    return format_;
  }

  // Returns the required fields for this rule.
  const std::vector<AddressField>& GetRequired() const { return required_; }

  // Returns the sub-keys for this rule, which are the administrative areas of a
  // country, the localities of an administrative area, or the dependent
  // localities of a locality. For example, the rules for "US" have sub-keys of
  // "CA", "NY", "TX", etc.
  const std::vector<std::string>& GetSubKeys() const { return sub_keys_; }

  // Returns all of the language codes for which this rule has custom rules, for
  // example ["de", "fr", "it"].
  const std::vector<std::string>& GetLanguages() const { return languages_; }

  // Returns the language code of this rule, for example "de".
  const std::string& GetLanguage() const { return language_; }

  // Returns the postal code format, for example "\\d{5}([ \\-]\\d{4})?".
  const std::string& GetPostalCodeFormat() const { return postal_code_format_; }

  // The message string identifier for admin area name. If not set, then
  // INVALID_MESSAGE_ID.
  int GetAdminAreaNameMessageId() const { return admin_area_name_message_id_; }

  // The error message string identifier for an invalid admin area. If not set,
  // then INVALID_MESSAGE_ID.
  int GetInvalidAdminAreaMessageId() const {
    return invalid_admin_area_message_id_;
  }

  // The message string identifier for postal code name. If not set, then
  // INVALID_MESSAGE_ID.
  int GetPostalCodeNameMessageId() const {
    return postal_code_name_message_id_;
  }

  // The error message string identifier for an invalid postal code. If not set,
  // then INVALID_MESSAGE_ID.
  int GetInvalidPostalCodeMessageId() const {
    return invalid_postal_code_message_id_;
  }

  // Returns the error message string identifier for an invalid |field|.
  int GetInvalidFieldMessageId(AddressField field) const;

 private:
  std::vector<std::vector<AddressField> > format_;
  std::vector<AddressField> required_;
  std::vector<std::string> sub_keys_;
  std::vector<std::string> languages_;
  std::string language_;
  std::string postal_code_format_;
  int admin_area_name_message_id_;
  int invalid_admin_area_message_id_;
  int postal_code_name_message_id_;
  int invalid_postal_code_message_id_;

  DISALLOW_COPY_AND_ASSIGN(Rule);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_RULE_H_
