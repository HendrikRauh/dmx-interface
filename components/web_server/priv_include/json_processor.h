/**
 * @file json_processor.h
 * @brief Header file for JSON processing utilities used in the web server
 * component.
 *
 * This file defines the structures and function prototypes for validating and
 * processing JSON data received by the web server. It provides a flexible
 * framework for defining validation rules and associated actions for different
 * types of JSON data, including booleans, numbers, strings, objects, and
 * arrays.
 */

#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Macro helpers to construct processors cleanly
 * @warning These macros use compound literals to generate pointers to inline
 * initialized structures (e.g. `&(json_processor_t){...}`).
 * Only use these macros in contexts where such usage is valid (e.g. global or
 * static file scope)
 * @{
 */

/**
 * @brief Macro to define a JSON processor for boolean fields
 */
#define JSON_PROC_BOOL(act_func)                                               \
  &(json_processor_t){.type = JSON_PROCESSOR_TYPE_BOOL,                        \
                      .processor.boolean = {.action = (act_func)}}

/**
 * @brief Macro to define a JSON processor for integer fields
 */
#define JSON_PROC_INT(min_val, max_val, act_func)                              \
  &(json_processor_t){.type = JSON_PROCESSOR_TYPE_INT,                         \
                      .processor.number = {.min = (min_val),                   \
                                           .max = (max_val),                   \
                                           .action = (act_func)}}

/**
 * @brief Macro to define a JSON processor for string fields
 */
#define JSON_PROC_STR(min_len, max_len, act_func)                              \
  &(json_processor_t){.type = JSON_PROCESSOR_TYPE_STRING,                      \
                      .processor.string = {.min_length = (min_len),            \
                                           .max_length = (max_len),            \
                                           .action = (act_func)}}

/**
 * @brief Macro to define a JSON processor for object fields
 */
#define JSON_PROC_OBJ(_processor, count, act_func)                             \
  &(json_processor_t){.type = JSON_PROCESSOR_TYPE_OBJECT,                      \
                      .processor.object = {.processors = (_processor),         \
                                           .num_processors = (count),          \
                                           .action = (act_func)}}

/**
 * @brief Macro to define a JSON processor for array fields
 */
#define JSON_PROC_ARR(_processor, min_len, max_len, act_func)                  \
  &(json_processor_t){.type = JSON_PROCESSOR_TYPE_ARRAY,                       \
                      .processor.array = {.min_items = (min_len),              \
                                          .max_items = (max_len),              \
                                          .item_processor = (_processor),      \
                                          .action = (act_func)}}

/** @} */

/**
 * @brief Types of JSON processors that can be defined for validating and
 * processing JSON data.
 */
typedef enum {
  JSON_PROCESSOR_TYPE_BOOL,   ///< Processor for boolean JSON values
  JSON_PROCESSOR_TYPE_INT,    ///< Processor for integer JSON values
  JSON_PROCESSOR_TYPE_STRING, ///< Processor for string JSON values
  JSON_PROCESSOR_TYPE_OBJECT, ///< Processor for JSON objects
  JSON_PROCESSOR_TYPE_ARRAY,  ///< Processor for JSON arrays
} json_processor_type_t;

typedef struct json_processor_t json_processor_t;

/**
 * @brief Validator and action for boolean JSON fields
 */
typedef struct {
  /** Optional pointer to a function that will be executed if the validation of
   * the whole JSON object passes */
  void (*action)(bool boolean);
} json_processor_boolean_t;

/**
 * @brief Validator and action for integer JSON fields
 */
typedef struct {
  int min; ///< Minimum acceptable value (inclusive)
  int max; ///< Maximum acceptable value (inclusive)
  /** Optional pointer to a function that will be executed if the validation of
   * the whole JSON object passes */
  void (*action)(int number);
} json_processor_number_t;

/**
 * @brief Validator and action for string JSON fields
 */
typedef struct {
  int min_length; ///< Minimum acceptable string length (inclusive)
  int max_length; ///< Maximum acceptable string length (inclusive)
  /** Optional pointer to a function that will be executed if the validation of
   * the whole JSON object passes */
  void (*action)(char *string);
} json_processor_string_t;

/**
 * @brief Validator and action for entries in JSON objects
 */
typedef struct {
  const char *key;     ///< JSON key to match in the object
  const bool required; ///< Whether this field is required (if true, validation
                       ///< fails if the key is missing)
  /**
   * Pointer to a processor that defines validation rules and actions for this
   * field's value
   */
  const json_processor_t *entry_processor;
} json_processor_object_entry_t;

/**
 * @brief Validator and action for JSON objects
 */
typedef struct {
  /**
   * Pointer to an array of json_processor_object_entry_t structures that define
   * the validation rules and actions for each field in the object.
   * Note: If the JSON object being validated contains keys that are not listed
   * in this array, the validation will fail with an error indicating the
   * presence of unexpected fields.
   */
  const json_processor_object_entry_t *processors;
  size_t num_processors; ///< Number of entries in the processors array
  /**
   * Optional pointer to a function that will be executed if the validation of
   * the whole JSON object passes. The function will receive the entire cJSON
   * object as an argument.
   */
  void (*action)(const cJSON *object);
} json_processor_object_t;

/**
 * @brief Validator and action for JSON arrays
 */
typedef struct {
  size_t min_items; ///< Minimum number of items in the array (inclusive)
  size_t max_items; ///< Maximum number of items in the array (inclusive)
  /**
   * Pointer to a processor that defines validation rules and actions for each
   * item in the array. Each item in the JSON array will be validated against
   * this processor.
   */
  const json_processor_t *item_processor;
  /**
   * Optional pointer to a function that will be executed if the validation of
   * the whole JSON object passes. The function will receive the entire cJSON
   * array as an argument.
   */
  void (*action)(const cJSON *array);
} json_processor_array_t;

/**
 * @brief Union of all possible JSON processor types
 */
typedef struct json_processor_t {
  json_processor_type_t type; ///< Type of processor, indicating which member of
                              ///< the union is valid

  union {
    json_processor_boolean_t boolean;
    json_processor_number_t number;
    json_processor_string_t string;
    json_processor_object_t object;
    json_processor_array_t array;
  } processor; ///< Union containing the specific processor definition based on
               ///< the type
} json_processor_t;

/**
 * @brief Validates a cJSON object against the provided json_processor_t
 * definition and executes any associated actions if the validation of the whole
 * JSON passes.
 * @param[in] json Pointer to the cJSON object to validate
 * @param[in] processor Pointer to the json_processor_t definition to validate
 * against
 * @param[out] out_err If validation fails, this will be set to point to a newly
 * allocated string containing an error message describing the validation
 * failure. The caller is responsible for freeing this string. If validation
 * passes, this will be set to `NULL`.
 * @return
 * - `ESP_OK` if validation passes
 *
 * - `ESP_INVALID_ARG` if validation fails due to the JSON not meeting the
 * specified rules
 *
 * - `ESP_ERR_INVALID_STATE` if the function is called with invalid arguments
 * (e.g. `out_err` is `NULL`)
 *
 * - `ESP_ERR_NO_MEM` if validation fails due to a memory allocation error
 */
esp_err_t json_processor_process(const cJSON *json,
                                 const json_processor_t *processor,
                                 char **out_err);

#ifdef __cplusplus
}
#endif
