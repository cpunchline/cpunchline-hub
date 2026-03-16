#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// https://bgithub.xyz/csepracticals/RPC
// https://www.bilibili.com/video/BV1ktSKYQEhh

/* ========== Client ========== */
/* Prepare the send data buffer */
/* Prepare serialized data buffer */
/* Fill the send data */
/* Step 1 : Invoke the rpc */
/* Step 2 : Serialize/Marshal the send data to the serialized data buffer */
/* Step 3 : Send the serialized data to the server, and wait for the reply */
/* free the serialized data buffer */
/* free the send data buffer */

/* Prepare the recv data buffer */
/* Step 9 : DeSerialize/Unmarshal the serialized data (result) recvd from Server to the recv data */
/* Client has successfully reconstructed the rpc result object from the recv data */
/* Step 10 : printf the RPC result */
/* free the recv data buffer */

/* ========== Server ========== */
/* Prepare the recv data buffer */
/* Step 4 : Recieve the Data from client */
/* Step 5 : DeSerialize/Unmarshal the serialized data (request) recvd from Client to the recv data */
/* Step 6 : Call the Actual RPC and return its result */
/* free the recv data buffer */

/* Prepare the result data buffer */
/* Prepare serialized data buffer */
/* Step 7 : Serialize/Marshal the result data to the serialized data buffer */
/* Step 8 : Send the serialized result data back to client */
/* free the serialized data buffer */
/* free the result data buffer */

#ifdef __cplusplus
}
#endif
