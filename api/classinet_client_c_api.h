/*
 * This is the Classinet client C API.
 * See https://classinet.com/api
 * Author: Yaniv Bargury yaniv@classinet.com
 * Version: 0.1
 * Copyright (c) 2022 Classinet Technologies LTD
 * Use of this file and product at this version is subject to the End User License Agreement which can be found in https://classinet.com/aula
 * Highlights from the EULA: End users may copy this file and use it in their product, but not to reverse engineer or claim ownership on it.
 *
 */

#ifndef CLASSINET_CLIENT_C_API_H
#define CLASSINET_CLIENT_C_API_H
#ifdef __cplusplus
extern "C" {
#endif

extern void Sonething();

#define CLASSINET_STRING_VALUE_LENGTH 256

// user_token - C string. Enables billing the usage costs. Get from classinet.com
// instance_description - C string. Optional, may be NULL or empty. User description of this instance to identify it in billing records
// return value is 0 if there was an error else returns 1. Return of 1 means the connection is ready for use. Use classinet_connection_status() to get information about the error.
extern int classinet_client_connect(const char *user_token, const char *instance_description);

// status - Return the status of the classinet client. Assumes a buffer of CLASSINET_RETURN_STRING_LENGTH chars was allocated by caller
// return value is 1 is the client connection is ready, else returns 0 and status is set to the description of the problem preventing service
extern int classinet_client_status(char *status);

// token is set the billing token that was given to create the connection of this client. Assumes a buffer of CLASSINET_RETURN_STRING_LENGTH chars was allocated by caller
extern void classinet_client_get_user_token(char *token);
// description is set the instance description that was given to create the connection of this client. Assumes a buffer of CLASSINET_RETURN_STRING_LENGTH chars was allocated by caller
extern void classinet_client_get_instance_description(char *decription);

// buffer_length is set by the caller to the number of chars allocated in the buffer pointed to by models.
// On return, it filled by the actual length consumed by the buffer including the trailing char(0).
// If the return value is the same as the input value then a new call should be made to this function with a larger buffer.
// models is filled by a C string describing the models available to the user. This includes the user private models and well as public models from classinet.
// todo: describe model list format.
// return value is 1 if the call succeeded else 0 when failed. In case it failed use  classinet_client_status() to learn more about the error.
extern int classinet_get_available_models(char *models, int *buffer_length);

// Similar to classinet_get_available_models() but returns just the metadata line for the model requested in the model parameter.
// return value is 1 if the call succeeded and 0 otherwise. Failure means this model is not available to this user, or the metadata buffer is too small. An error description would be assigned to the metadata buffer.
extern int classinet_get_model_metadata(const char *model, char *metadata, size_t *buffer_length);

extern int classinet_register_model(const char *metadata, const char *binary_model, const size_t binary_model_length);

// return 0 if there was not enough inference_buffer_length to contain the inference or if detections is NULL
// otherwise return 1, and any error is returned as the detection.
// Note inference_buffer_length must be at least CLASSINET_RETURN_STRING_LENGTH
// user_context is optional and is limited to 128 characters. Anything longer would return an error detection.
extern int classinet_infer(const char *model_name_p, const char *binary_image, const size_t binary_image_length, const char *user_context, char *inference, size_t *inference_buffer_length);

// Always calls the callback, with either the inference or an error. Unless the callback is left NULL in which case it does nothing
// inference_context is an optional pointer sent back with the callback so that the user can have some context information about the result in the callback.
extern void classinet_async_infer(const char *model_name, const char *binary_image, const size_t binary_image_length, const char *user_context, const void *inference_context, void (*callback)(const void *inference_context, const char *inference));

extern void classinet_debug_hints_set(const char *hints);

extern void classinet_debug_hints_get(const char *key, char *return_value);

//extern "C" void Classinet_Break(char* x);
//extern "C" void Classinet_ValidateV2(const char* billing_code, const char* product, const char* instance_id, char* result);
//extern "C" void Classinet_Status(char* result);
//extern "C" void Classinet_HeadsUp(const char* url, const char* referrer, const char* user_id, const char* case_id, char* result);
//extern "C" void Classinet_ProcessImage(const char* options, const char* url, const char* referrer, const char* user_id, const char* case_id, uint32_t binary_image_length, const char* binary_image, size_t * result_length, char* result);
//extern "C" void Classinet_ClassifyV2(const char* url, const char* referrer, const char* user_id, const char* case_id, uint32_t binary_image_length, const char* binary_image, char* result);
//extern "C" void Classinet_BlockImage(uint32_t style, uint32_t binary_image_length, const char* binary_image, uint32_t result_buffer_size, uint32_t *result_image_length, char* result);
//extern "C" void Classinet_FullStatus(const char* what, uint32_t result_buffer_size, char* result);

#endif
#ifdef __cplusplus
}
#endif
