/**
 * @brief Implementation of JSON processing utilities used in the web server
 * component.
 *
 * This file defines the functions for validating and processing JSON data
 * received by the web server. It provides a flexible framework for defining
 * validation rules and associated actions for different types of JSON data,
 * including booleans, numbers, strings, objects, and arrays.
 */

#define LOG_TAG "JSON_PROC" ///< Logging tag for this file

#include "json_processor.h"
#include "logger.h"
#include "stdio.h"
#include "string.h"

static esp_err_t validate(const cJSON *json, const json_processor_t *processor,
                          char **out_err);

/**
 * @brief Validate a boolean value against the specified rules
 * @param[in] boolean The cJSON boolean value to validate
 * @param[in] rules The validation rules for the boolean value
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate_boolean(const cJSON *boolean,
                                  json_processor_boolean_t rules,
                                  char **out_err) {
  if (!cJSON_IsBool(boolean)) {
    *out_err = strdup("Value is not a boolean");
    return (*out_err == NULL) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

/**
 * @brief Validate a number value against the specified rules
 * @param[in] number The cJSON number value to validate
 * @param[in] rules The validation rules for the number value
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate_number(const cJSON *number,
                                 json_processor_number_t rules,
                                 char **out_err) {
  if (!cJSON_IsNumber(number)) {
    *out_err = strdup("Value is not a number");
    return (*out_err == NULL) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
  }

  if (number->valueint < rules.min || number->valueint > rules.max) {
    if (asprintf(out_err, "Expected number between %d and %d, got %d",
                 rules.min, rules.max, number->valueint) < 0) {
      *out_err = NULL;
      return ESP_ERR_NO_MEM;
    }
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

/**
 * @brief Validate a string value against the specified rules
 * @param[in] string The cJSON string value to validate
 * @param[in] rules The validation rules for the string value
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate_string(const cJSON *string,
                                 json_processor_string_t rules,
                                 char **out_err) {
  if (!cJSON_IsString(string) || string->valuestring == NULL) {
    *out_err = strdup("Value is not a string");
    return (*out_err == NULL) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
  }

  size_t len = strlen(string->valuestring);
  if (len < rules.min_length || len > rules.max_length) {
    if (asprintf(
            out_err,
            "Expected string with length between %d and %d, got length %zu",
            rules.min_length, rules.max_length, len) < 0) {
      *out_err = NULL;
      return ESP_ERR_NO_MEM;
    }
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

/**
 * @brief Validate an object against the specified rules
 * @param[in] object The cJSON object to validate
 * @param[in] rules The validation rules for the object
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate_object(const cJSON *object,
                                 json_processor_object_t rules,
                                 char **out_err) {
  if (!cJSON_IsObject(object)) {
    *out_err = strdup("Value is not an object");
    return (*out_err == NULL) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
  }

  const cJSON *field = NULL;
  cJSON_ArrayForEach(field, object) {
    const json_processor_object_entry_t *matched_rule = NULL;

    // find matching processor rule for this field
    for (size_t i = 0; i < rules.num_processors; i++) {
      if (strcmp(field->string, rules.processors[i].key) == 0) {
        matched_rule = &rules.processors[i];
        break;
      }
    }

    if (matched_rule == NULL) {
      if (asprintf(out_err, "Unexpected field '%s' in object", field->string) <
          0) {
        *out_err = NULL;
        return ESP_ERR_NO_MEM;
      }
      return ESP_ERR_INVALID_ARG;
    }

    char *nested_err = NULL;
    esp_err_t res = validate(field, matched_rule->entry_processor, &nested_err);

    if (res != ESP_OK) {
      if (res == ESP_ERR_NO_MEM) {
        *out_err = NULL;
        return ESP_ERR_NO_MEM;
      }

      if (asprintf(out_err, "Field '%s': %s", matched_rule->key, nested_err) <
          0) {
        *out_err = NULL;
        free(nested_err);
        return ESP_ERR_NO_MEM;
      }

      free(nested_err);
      return ESP_ERR_INVALID_ARG;
    }
  }

  // check if all required fields are present
  for (size_t i = 0; i < rules.num_processors; i++) {
    if (!rules.processors[i].required) {
      continue;
    }

    const char *key = rules.processors[i].key;
    const cJSON *field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (field == NULL) {
      if (asprintf(out_err, "Missing required field '%s' in object", key) < 0) {
        *out_err = NULL;
        return ESP_ERR_NO_MEM;
      }
      return ESP_ERR_INVALID_ARG;
    }
  }

  return ESP_OK;
}

/**
 * @brief Validate an array against the specified rules
 * @param[in] array The cJSON array to validate
 * @param[in] rules The validation rules for the array
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate_array(const cJSON *array,
                                json_processor_array_t rules, char **out_err) {
  if (!cJSON_IsArray(array)) {
    *out_err = strdup("Value is not an array");
    return (*out_err == NULL) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
  }

  int len = cJSON_GetArraySize(array);

  if (len < rules.min_items || len > rules.max_items) {
    if (asprintf(
            out_err,
            "Expected array with length between %zu and %zu, got length %d",
            rules.min_items, rules.max_items, len) < 0) {
      *out_err = NULL;
      return ESP_ERR_NO_MEM;
    }
    return ESP_ERR_INVALID_ARG;
  }

  for (int i = 0; i < len; i++) {
    cJSON *item = cJSON_GetArrayItem(array, i);

    char *nested_err;
    esp_err_t res = validate(item, rules.item_processor, &nested_err);

    if (res != ESP_OK) {
      if (res == ESP_ERR_NO_MEM) {
        *out_err = NULL;
        return ESP_ERR_NO_MEM;
      }

      if (asprintf(out_err, "Array index %d: %s", i, nested_err) < 0) {
        *out_err = NULL;
        free(nested_err);
        return ESP_ERR_NO_MEM;
      }
      free(nested_err);
      return ESP_ERR_INVALID_ARG;
    }
  }

  return ESP_OK;
}

/**
 * @brief Validate a JSON value against the specified processor
 * @param[in] json The cJSON value to validate
 * @param[in] processor The validation processor
 * @param[out] out_err Pointer to a string where any error message will be
 * stored
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t validate(const cJSON *json, const json_processor_t *processor,
                          char **out_err) {
  switch (processor->type) {
  case JSON_PROCESSOR_TYPE_BOOL:
    return validate_boolean(json, processor->processor.boolean, out_err);
  case JSON_PROCESSOR_TYPE_INT:
    return validate_number(json, processor->processor.number, out_err);
  case JSON_PROCESSOR_TYPE_STRING:
    return validate_string(json, processor->processor.string, out_err);
  case JSON_PROCESSOR_TYPE_OBJECT:
    return validate_object(json, processor->processor.object, out_err);
  case JSON_PROCESSOR_TYPE_ARRAY:
    return validate_array(json, processor->processor.array, out_err);
  default:
    LOGE("Invalid processor type, cannot validate JSON");
    *out_err = NULL;
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

/**
 * @brief Execute actions associated with a JSON value based on the specified
 * processor
 * @param[in] json The cJSON value for which to execute actions
 * @param[in] processor The processor containing the actions to execute
 */
static void execute_actions(const cJSON *json,
                            const json_processor_t *processor) {
  switch (processor->type) {
  case JSON_PROCESSOR_TYPE_BOOL:
    if (processor->processor.boolean.action != NULL) {
      processor->processor.boolean.action(cJSON_IsTrue(json));
    }
    break;
  case JSON_PROCESSOR_TYPE_INT:
    if (processor->processor.number.action != NULL) {
      processor->processor.number.action(json->valueint);
    }
    break;
  case JSON_PROCESSOR_TYPE_STRING:
    if (processor->processor.string.action != NULL) {
      processor->processor.string.action(json->valuestring);
    }
    break;
  case JSON_PROCESSOR_TYPE_OBJECT:
    if (processor->processor.object.action != NULL) {
      processor->processor.object.action(json);
    }

    for (size_t i = 0; i < processor->processor.object.num_processors; i++) {
      const json_processor_object_entry_t entry =
          processor->processor.object.processors[i];
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, entry.key);

      if (item == NULL) {
        // optional field is missing, skip
        continue;
      }
      execute_actions(item, entry.entry_processor);
    }
    break;
  case JSON_PROCESSOR_TYPE_ARRAY:
    if (processor->processor.array.action != NULL) {
      processor->processor.array.action(json);
    }

    int len = cJSON_GetArraySize(json);
    for (size_t i = 0; i < len; i++) {
      cJSON *item = cJSON_GetArrayItem(json, i);
      execute_actions(item, processor->processor.array.item_processor);
    }
    break;
  default:
    LOGE("Invalid processor type, cannot execute action");
    break;
  }
}

esp_err_t json_processor_process(const cJSON *json,
                                 const json_processor_t *processor,
                                 char **out_err) {
  if (out_err == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  *out_err = NULL;

  esp_err_t validation_result = validate(json, processor, out_err);

  if (validation_result != ESP_OK) {
    return validation_result;
  }

  execute_actions(json, processor);
  return ESP_OK;
}
