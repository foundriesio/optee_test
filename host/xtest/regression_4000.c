/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <malloc.h>
#include <time.h>

#include "xtest_test.h"
#include "xtest_helpers.h"

#include <tee_api_types.h>
#include <tee_api_defines_extensions.h>
#include <ta_crypt.h>
#include <utee_defines.h>
#include <util.h>

#include <regression_4000_data.h>
#include <nist/186-2ecdsatestvectors.h>

#ifdef CFG_SECURE_KEY_SERVICES
#include <pkcs11.h>
#include <sks_ck_debug.h>
#endif

#include <assert.h>

#ifdef CFG_SECURE_KEY_SERVICES
int ck_ec_params_attr_from_tee_curve(CK_ATTRIBUTE *attrs, size_t count,
					 uint32_t curve);
int ck_ec_params_attr_from_tee_algo(CK_ATTRIBUTE *attrs, size_t count,
					 uint32_t algo);

/*
 * Load an attribute value (value data and value size) in a PKCS#11 object
 */
static int set_ck_attr(CK_ATTRIBUTE *attrs, size_t count, CK_ULONG id,
			CK_VOID_PTR data, CK_ULONG size)
{
	size_t idx;

	for (idx = 0; idx < count; idx++) {
		if (attrs[idx].type == id) {
			attrs[idx].pValue = data;
			attrs[idx].ulValueLen = size;
			return 0;
		}
	}

	return 1;
}

#define SET_CK_ATTR(attrs, id, data, size) \
	set_ck_attr((CK_ATTRIBUTE *)attrs, ARRAY_SIZE(attrs), id, \
			(CK_VOID_PTR)data, (CK_ULONG)size)

/*
 * DER encoding of elliptic curves supported by the
 * GPD TEE Core Internal API v1.2
 */
static const uint8_t nist_secp192r1_der[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x01
};
static const uint8_t nist_secp224r1_der[] = {
	0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x21
};
static const uint8_t nist_secp256r1_der[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};
static const uint8_t nist_secp384r1_der[] = {
	0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22
};
static const uint8_t nist_secp521r1_der[] = {
	0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23
};

int ck_ec_params_attr_from_tee_curve(CK_ATTRIBUTE *attrs, size_t count,
					 uint32_t curve)
{
	void *der;
	size_t size;

	switch (curve) {
	case TEE_ECC_CURVE_NIST_P192:
		der = (void *)nist_secp192r1_der;
		size = sizeof(nist_secp192r1_der);
		break;
	case TEE_ECC_CURVE_NIST_P224:
		der = (void *)nist_secp224r1_der;
		size = sizeof(nist_secp224r1_der);
		break;
	case TEE_ECC_CURVE_NIST_P256:
		der = (void *)nist_secp256r1_der;
		size = sizeof(nist_secp256r1_der);
		break;
	case TEE_ECC_CURVE_NIST_P384:
		der = (void *)nist_secp384r1_der;
		size = sizeof(nist_secp384r1_der);
		break;
	case TEE_ECC_CURVE_NIST_P521:
		der = (void *)nist_secp521r1_der;
		size = sizeof(nist_secp521r1_der);
		break;
	default:
		return -1;
	}

	return set_ck_attr(attrs, count, CKA_EC_PARAMS, der, size);
}

int ck_ec_params_attr_from_tee_algo(CK_ATTRIBUTE *attrs, size_t count,
					 uint32_t algo)
{
	void *der;
	size_t size;

	switch (algo) {
	case TEE_ALG_ECDSA_P192:
		der = (void *)nist_secp192r1_der;
		size = sizeof(nist_secp192r1_der);
		break;
	case TEE_ALG_ECDSA_P224:
		der = (void *)nist_secp224r1_der;
		size = sizeof(nist_secp224r1_der);
		break;
	case TEE_ALG_ECDSA_P256:
		der = (void *)nist_secp256r1_der;
		size = sizeof(nist_secp256r1_der);
		break;
	case TEE_ALG_ECDSA_P384:
		der = (void *)nist_secp384r1_der;
		size = sizeof(nist_secp384r1_der);
		break;
	case TEE_ALG_ECDSA_P521:
		der = (void *)nist_secp521r1_der;
		size = sizeof(nist_secp521r1_der);
		break;
	default:
		return -1;
	}

	return set_ck_attr(attrs, count, CKA_EC_PARAMS, der, size);
}
#endif /*CFG_SECURE_KEY_SERVICES*/

static TEEC_Result ta_crypt_cmd_reset_operation(ADBG_Case_t *c, TEEC_Session *s,
						TEE_OperationHandle oph)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE,
					 TEEC_NONE);
	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_RESET_OPERATION, &op,
				 &ret_orig);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}
	return res;
}

static TEEC_Result ta_crypt_cmd_copy_operation(ADBG_Case_t *c,
					       TEEC_Session *s,
					       TEE_OperationHandle dst_oph,
					       TEE_OperationHandle src_oph)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)dst_oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)dst_oph;

	assert((uintptr_t)src_oph <= UINT32_MAX);
	op.params[0].value.b = (uint32_t)(uintptr_t)src_oph;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_COPY_OPERATION, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}
	return res;
}

static TEEC_Result ta_crypt_cmd_digest_update(ADBG_Case_t *c, TEEC_Session *s,
					      TEE_OperationHandle oph,
					      const void *chunk,
					      size_t chunk_size)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;
	op.params[1].tmpref.buffer = (void *)chunk;
	op.params[1].tmpref.size = chunk_size;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_DIGEST_UPDATE, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
			    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_digest_do_final(ADBG_Case_t *c, TEEC_Session *s,
						TEE_OperationHandle oph,
						const void *chunk,
						size_t chunk_len, void *hash,
						size_t *hash_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)chunk;
	op.params[1].tmpref.size = chunk_len;

	op.params[2].tmpref.buffer = (void *)hash;
	op.params[2].tmpref.size = *hash_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_DIGEST_DO_FINAL, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*hash_len = op.params[2].tmpref.size;

	return res;
}

static TEE_Result ta_crypt_cmd_set_operation_key2(ADBG_Case_t *c,
						  TEEC_Session *s,
						  TEE_OperationHandle oph,
						  TEE_ObjectHandle key1,
						  TEE_ObjectHandle key2)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	assert((uintptr_t)key1 <= UINT32_MAX);
	op.params[0].value.b = (uint32_t)(uintptr_t)key1;

	assert((uintptr_t)key2 <= UINT32_MAX);
	op.params[1].value.a = (uint32_t)(uintptr_t)key2;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_VALUE_INPUT,
					 TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_SET_OPERATION_KEY2, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
			    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_mac_init(ADBG_Case_t *c, TEEC_Session *s,
					 TEE_OperationHandle oph,
					 const void *iv, size_t iv_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	if (iv != NULL) {
		op.params[1].tmpref.buffer = (void *)iv;
		op.params[1].tmpref.size = iv_len;
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
						 TEEC_MEMREF_TEMP_INPUT,
						 TEEC_NONE, TEEC_NONE);
	} else {
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_NONE,
						 TEEC_NONE, TEEC_NONE);
	}

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_MAC_INIT, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_mac_update(ADBG_Case_t *c, TEEC_Session *s,
					   TEE_OperationHandle oph,
					   const void *chunk, size_t chunk_size)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)chunk;
	op.params[1].tmpref.size = chunk_size;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_MAC_UPDATE, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_mac_final_compute(ADBG_Case_t *c,
						  TEEC_Session *s,
						  TEE_OperationHandle oph,
						  const void *chunk,
						  size_t chunk_len,
						  void *hash,
						  size_t *hash_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)chunk;
	op.params[1].tmpref.size = chunk_len;

	op.params[2].tmpref.buffer = (void *)hash;
	op.params[2].tmpref.size = *hash_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_MAC_FINAL_COMPUTE, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*hash_len = op.params[2].tmpref.size;

	return res;
}

static TEEC_Result ta_crypt_cmd_cipher_init(ADBG_Case_t *c, TEEC_Session *s,
					    TEE_OperationHandle oph,
					    const void *iv, size_t iv_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	if (iv != NULL) {
		op.params[1].tmpref.buffer = (void *)iv;
		op.params[1].tmpref.size = iv_len;

		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
						 TEEC_MEMREF_TEMP_INPUT,
						 TEEC_NONE, TEEC_NONE);
	} else {
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_NONE,
						 TEEC_NONE, TEEC_NONE);
	}

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_CIPHER_INIT, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_cipher_update(ADBG_Case_t *c, TEEC_Session *s,
					      TEE_OperationHandle oph,
					      const void *src, size_t src_len,
					      void *dst, size_t *dst_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)src;
	op.params[1].tmpref.size = src_len;

	op.params[2].tmpref.buffer = dst;
	op.params[2].tmpref.size = *dst_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_CIPHER_UPDATE, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*dst_len = op.params[2].tmpref.size;

	return res;
}

static TEEC_Result ta_crypt_cmd_cipher_do_final(ADBG_Case_t *c,
						TEEC_Session *s,
						TEE_OperationHandle oph,
						const void *src,
						size_t src_len,
						void *dst,
						size_t *dst_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)src;
	op.params[1].tmpref.size = src_len;

	op.params[2].tmpref.buffer = (void *)dst;
	op.params[2].tmpref.size = *dst_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_CIPHER_DO_FINAL, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
			    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*dst_len = op.params[2].tmpref.size;

	return res;
}

static TEEC_Result ta_crypt_cmd_random_number_generate(ADBG_Case_t *c,
						       TEEC_Session *s,
						       void *buf,
						       size_t blen)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	op.params[0].tmpref.buffer = buf;
	op.params[0].tmpref.size = blen;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_RANDOM_NUMBER_GENEREATE, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	(void)ADBG_EXPECT_COMPARE_UNSIGNED(c, blen, ==,
					   op.params[0].tmpref.size);
	return res;
}

static TEEC_Result ta_crypt_cmd_ae_init(ADBG_Case_t *c, TEEC_Session *s,
					TEE_OperationHandle oph,
					const void *nonce, size_t nonce_len,
					size_t tag_len, size_t aad_len,
					size_t payload_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;
	op.params[0].value.b = tag_len;

	op.params[1].tmpref.buffer = (void *)nonce;
	op.params[1].tmpref.size = nonce_len;

	op.params[2].value.a = aad_len;
	op.params[2].value.b = payload_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_VALUE_INPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_AE_INIT, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}
	return res;
}

static TEEC_Result ta_crypt_cmd_ae_update_aad(ADBG_Case_t *c, TEEC_Session *s,
					      TEE_OperationHandle oph,
					      const void *aad, size_t aad_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)aad;
	op.params[1].tmpref.size = aad_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_AE_UPDATE_AAD, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_ae_update(ADBG_Case_t *c,
					  TEEC_Session *s,
					  TEE_OperationHandle oph,
					  const void *src,
					  size_t src_len,
					  void *dst,
					  size_t *dst_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)src;
	op.params[1].tmpref.size = src_len;

	op.params[2].tmpref.buffer = (void *)dst;
	op.params[2].tmpref.size = *dst_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_AE_UPDATE, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*dst_len = op.params[2].tmpref.size;

	return res;
}

static TEEC_Result ta_crypt_cmd_ae_encrypt_final(ADBG_Case_t *c,
						 TEEC_Session *s,
						 TEE_OperationHandle oph,
						 const void *src,
						 size_t src_len, void *dst,
						 size_t *dst_len, void *tag,
						 size_t *tag_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)src;
	op.params[1].tmpref.size = src_len;

	op.params[2].tmpref.buffer = (void *)dst;
	op.params[2].tmpref.size = *dst_len;

	op.params[3].tmpref.buffer = (void *)tag;
	op.params[3].tmpref.size = *tag_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_MEMREF_TEMP_OUTPUT);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_AE_ENCRYPT_FINAL, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS) {
		*dst_len = op.params[2].tmpref.size;
		*tag_len = op.params[3].tmpref.size;
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_ae_decrypt_final(ADBG_Case_t *c,
						 TEEC_Session *s,
						 TEE_OperationHandle oph,
						 const void *src, size_t src_len,
						 void *dst, size_t *dst_len,
						 const void *tag, size_t tag_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = (void *)src;
	op.params[1].tmpref.size = src_len;

	op.params[2].tmpref.buffer = dst;
	op.params[2].tmpref.size = *dst_len;

	op.params[3].tmpref.buffer = (void *)tag;
	op.params[3].tmpref.size = tag_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_MEMREF_TEMP_INPUT);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_AE_DECRYPT_FINAL, &op,
				 &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*dst_len = op.params[2].tmpref.size;

	return res;
}

static TEEC_Result ta_crypt_cmd_asymmetric_operate(ADBG_Case_t *c,
						   TEEC_Session *s,
						   TEE_OperationHandle oph,
						   uint32_t cmd,
						   const TEE_Attribute *params,
						   uint32_t paramCount,
						   const void *src,
						   size_t src_len,
						   void *dst,
						   size_t *dst_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;
	uint8_t *buf;
	size_t blen;

	res = pack_attrs(params, paramCount, &buf, &blen);
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, res))
		return res;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = blen;

	op.params[2].tmpref.buffer = (void *)src;
	op.params[2].tmpref.size = src_len;

	op.params[3].tmpref.buffer = dst;
	op.params[3].tmpref.size = *dst_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT);

	res = TEEC_InvokeCommand(s, cmd, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS)
		*dst_len = op.params[3].tmpref.size;

	free(buf);
	return res;
}

static TEEC_Result ta_crypt_cmd_asymmetric_encrypt(ADBG_Case_t *c,
						   TEEC_Session *s,
						   TEE_OperationHandle oph,
						   const TEE_Attribute *params,
						   uint32_t paramCount,
						   const void *src,
						   size_t src_len,
						   void *dst,
						   size_t *dst_len)
{
	return ta_crypt_cmd_asymmetric_operate(c, s, oph,
					       TA_CRYPT_CMD_ASYMMETRIC_ENCRYPT,
					       params, paramCount,
					       src, src_len, dst, dst_len);
}

static TEEC_Result ta_crypt_cmd_asymmetric_decrypt(ADBG_Case_t *c,
						   TEEC_Session *s,
						   TEE_OperationHandle oph,
						   const TEE_Attribute *params,
						   uint32_t paramCount,
						   const void *src,
						   size_t src_len,
						   void *dst,
						   size_t *dst_len)
{
	return ta_crypt_cmd_asymmetric_operate(c, s, oph,
					       TA_CRYPT_CMD_ASYMMETRIC_DECRYPT,
					       params, paramCount,
					       src, src_len, dst, dst_len);
}

static TEEC_Result ta_crypt_cmd_asymmetric_sign(ADBG_Case_t *c,
						TEEC_Session *s,
						TEE_OperationHandle oph,
						const TEE_Attribute *params,
						uint32_t paramCount,
						const void *digest,
						size_t digest_len,
						void *signature,
						size_t *signature_len)
{
	return ta_crypt_cmd_asymmetric_operate(c, s, oph,
			TA_CRYPT_CMD_ASYMMETRIC_SIGN_DIGEST, params, paramCount,
			digest, digest_len, signature, signature_len);
}

static TEEC_Result ta_crypt_cmd_asymmetric_verify(ADBG_Case_t *c,
						  TEEC_Session *s,
						  TEE_OperationHandle oph,
						  const TEE_Attribute *params,
						  uint32_t paramCount,
						  const void *digest,
						  size_t digest_len,
						  const void *signature,
						  size_t signature_len)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;
	uint8_t *buf;
	size_t blen;

	res = pack_attrs(params, paramCount, &buf, &blen);
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, res))
		return res;

	assert((uintptr_t)oph <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)oph;

	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = blen;

	op.params[2].tmpref.buffer = (void *)digest;
	op.params[2].tmpref.size = digest_len;

	op.params[3].tmpref.buffer = (void *)signature;
	op.params[3].tmpref.size = signature_len;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_ASYMMETRIC_VERIFY_DIGEST,
				 &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	free(buf);
	return res;
}

static TEEC_Result ta_crypt_cmd_get_object_value_attribute(ADBG_Case_t *c,
							   TEEC_Session *s,
							   TEE_ObjectHandle o,
							   uint32_t attr_id,
							   uint32_t *valuea,
							   uint32_t *valueb)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;

	assert((uintptr_t)o <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)o;
	op.params[0].value.b = attr_id;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_VALUE_OUTPUT,
					 TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_GET_OBJECT_VALUE_ATTRIBUTE,
				 &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	if (res == TEEC_SUCCESS) {
		*valuea = op.params[1].value.a;
		*valueb = op.params[1].value.b;
	}

	return res;
}

static TEEC_Result ta_crypt_cmd_generate_key(ADBG_Case_t *c,
					     TEEC_Session *s,
					     TEE_ObjectHandle o,
					     uint32_t key_size,
					     const TEE_Attribute *params,
					     uint32_t paramCount)
{
	TEEC_Result res;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	uint32_t ret_orig;
	uint8_t *buf;
	size_t blen;

	res = pack_attrs(params, paramCount, &buf, &blen);
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, res))
		return res;

	assert((uintptr_t)o <= UINT32_MAX);
	op.params[0].value.a = (uint32_t)(uintptr_t)o;
	op.params[0].value.b = key_size;

	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = blen;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(s, TA_CRYPT_CMD_GENERATE_KEY, &op, &ret_orig);

	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, TEEC_ORIGIN_TRUSTED_APP,
						    ret_orig);
	}

	free(buf);
	return res;
}

static const uint8_t hash_data_md5_in1[] = {
	'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'
};

static const uint8_t hash_data_md5_out1[] = {
	0x61, 0x12, 0x71, 0x83, 0x70, 0x8d, 0x3a, 0xc7,
	0xf1, 0x9b, 0x66, 0x06, 0xfc, 0xae, 0x7d, 0xf6
};

static const uint8_t hash_data_sha1_in1[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'
};

static const uint8_t hash_data_sha1_out1[] = {
	0x4b, 0x98, 0x92, 0xb6, 0x52, 0x72, 0x14, 0xaf,
	0xc6, 0x55, 0xb8, 0xaa, 0x52, 0xf4, 0xd2, 0x03,
	0xc1, 0x5e, 0x7c, 0x9c
};

static const uint8_t hash_data_sha224_in1[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'
};

static const uint8_t hash_data_sha224_out1[] = {
	0x08, 0x21, 0x69, 0xf9, 0x77, 0x1b, 0x80, 0x15,
	0xf3, 0x97, 0xae, 0xde, 0x5b, 0xba, 0xa2, 0x72,
	0x2d, 0x8f, 0x5c, 0x19, 0xfe, 0xd2, 0xe2, 0x68,
	0x92, 0x49, 0xd8, 0x44
};

static const uint8_t hash_data_sha256_in1[] = { 'a', 'b', 'c' };

static const uint8_t hash_data_sha256_out1[] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
	0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
	0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
};

static const uint8_t hash_data_sha256_in2[] = { 'e', 'f', 'g' };

static const uint8_t hash_data_sha256_out2[] = {
	0xd4, 0xff, 0xe8, 0xe9, 0xee, 0x0b, 0x48, 0xeb,
	0xa7, 0x16, 0x70, 0x61, 0x23, 0xa7, 0x18, 0x7f,
	0x32, 0xea, 0xe3, 0xbd, 0xcb, 0x0e, 0x77, 0x63,
	0xe4, 0x1e, 0x53, 0x32, 0x67, 0xbd, 0x8a, 0x53
};


static const uint8_t hash_data_sha384_in1[] = {
	'a', 'b', 'c', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'
};

static const uint8_t hash_data_sha384_out1[] = {
	0x4c, 0xab, 0x80, 0x9d, 0x96, 0x84, 0x01, 0x47,
	0x67, 0x0a, 0xc1, 0x7a, 0xb6, 0xb9, 0xf7, 0x6e,
	0x35, 0xa6, 0xb0, 0x8c, 0xf5, 0x2a, 0x3d, 0x64,
	0x9a, 0x8c, 0x7e, 0x0c, 0x55, 0x45, 0xd3, 0x7d,
	0x1f, 0x7f, 0x28, 0x34, 0x96, 0x14, 0x44, 0x2a,
	0xf5, 0x98, 0xa2, 0x95, 0x24, 0x76, 0x53, 0x97
};

static const uint8_t hash_data_sha512_in1[] = {
	'a', 'b', 'c', 'd', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'
};

static const uint8_t hash_data_sha512_out1[] = {
	0x20, 0xd8, 0x64, 0x4f, 0x54, 0xa2, 0x5f, 0x6f,
	0x0a, 0xf9, 0xd5, 0x27, 0x7d, 0x17, 0xa8, 0x99,
	0x4c, 0x64, 0x3f, 0xd0, 0xf3, 0x83, 0x36, 0xee,
	0x93, 0x12, 0x55, 0xcd, 0x2e, 0x12, 0x34, 0xa0,
	0xc2, 0xaa, 0xf9, 0xbb, 0x15, 0xc5, 0xe9, 0xfa,
	0xf7, 0xa7, 0xda, 0xb8, 0x2f, 0x72, 0xa0, 0x47,
	0xe3, 0x02, 0x04, 0xe8, 0xa0, 0x35, 0x0c, 0x96,
	0x26, 0xd1, 0xcb, 0x8b, 0x47, 0x45, 0x25, 0xd0
};

struct xtest_hash_case {
	uint32_t algo;
	size_t in_incr;
	const uint8_t *in;
	size_t in_len;
	const uint8_t *out;
	size_t out_len;
};

#define XTEST_HASH_CASE(algo, in_incr, in, out) \
	{ (algo), (in_incr), (in), ARRAY_SIZE(in), (out), ARRAY_SIZE(out) }

static const struct xtest_hash_case hash_cases[] = {
	XTEST_HASH_CASE(TEE_ALG_MD5, 6, hash_data_md5_in1,
			hash_data_md5_out1),
	XTEST_HASH_CASE(TEE_ALG_SHA1, 3, hash_data_sha1_in1,
			hash_data_sha1_out1),
	XTEST_HASH_CASE(TEE_ALG_SHA224, 7, hash_data_sha224_in1,
			hash_data_sha224_out1),
	XTEST_HASH_CASE(TEE_ALG_SHA256, 1, hash_data_sha256_in1,
			hash_data_sha256_out1),
	XTEST_HASH_CASE(TEE_ALG_SHA256, 1, hash_data_sha256_in2,
			hash_data_sha256_out2),
	XTEST_HASH_CASE(TEE_ALG_SHA384, 1, hash_data_sha384_in1,
			hash_data_sha384_out1),
	XTEST_HASH_CASE(TEE_ALG_SHA512, 1, hash_data_sha512_in1,
			hash_data_sha512_out1),
};

static void xtest_tee_test_4001(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	size_t n;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;


	for (n = 0; n < ARRAY_SIZE(hash_cases); n++) {
		TEE_OperationHandle op1;
		TEE_OperationHandle op2;
		uint8_t out[64];
		size_t out_size;

		Do_ADBG_BeginSubCase(c, "Hash case %d algo 0x%x",
				     (int)n, (unsigned int)hash_cases[n].algo);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op1,
							hash_cases[n].algo,
							TEE_MODE_DIGEST, 0)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op2,
							hash_cases[n].algo,
							TEE_MODE_DIGEST, 0)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_digest_update(c, &session, op1,
						   hash_cases[n].in,
						   hash_cases[n].in_incr)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_copy_operation(c, &session, op2, op1)))
			goto out;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_digest_do_final(c, &session, op2,
				hash_cases[n].in + hash_cases[n].in_incr,
				hash_cases[n].in_len - hash_cases[n].in_incr,
				out, &out_size)))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, hash_cases[n].out,
					 hash_cases[n].out_len, out, out_size);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_reset_operation(c, &session, op1)))
			goto out;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_digest_do_final(c, &session, op1,
						     hash_cases[n].in,
						     hash_cases[n].in_len, out,
						     &out_size)))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, hash_cases[n].out,
					 hash_cases[n].out_len, out, out_size);

		/*
		 * Invoke TEE_DigestDoFinal() a second time to check that state
		 * was properly reset
		 */
		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_digest_do_final(c, &session, op1,
						     hash_cases[n].in,
						     hash_cases[n].in_len, out,
						     &out_size)))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, hash_cases[n].out,
					 hash_cases[n].out_len, out, out_size);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op1)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op2)))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
	}

out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4001, xtest_tee_test_4001,
		"Test TEE Internal API hash operations");

static const uint8_t mac_data_md5_key1[10] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_md5_in1[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_md5_out1[] = {
	0x80, 0x07, 0x07, 0x13, 0x46, 0x3e, 0x77, 0x49,
	0xb9, 0x0c, 0x2d, 0xc2, 0x49, 0x11, 0xe2, 0x75
};


/* generated with scripts/digest_hmac.pl */
static const uint8_t mac_data_sha1_key1[10] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_sha1_in1[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_sha1_out1[] = {
	0xDE, 0x7C, 0x9B, 0x85, 0xB8, 0xB7, 0x8A, 0xA6, /* .|...... */
	0xBC, 0x8A, 0x7A, 0x36, 0xF7, 0x0A, 0x90, 0x70, /* ..z6...p */
	0x1C, 0x9D, 0xB4, 0xD9,                         /* .... */
};

static const uint8_t mac_data_sha224_key1[24] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_sha224_in1[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_sha224_out1[] = {
	0x88, 0xFF, 0x8B, 0x54, 0x67, 0x5D, 0x39, 0xB8, /* ...Tg]9. */
	0xF7, 0x23, 0x22, 0xE6, 0x5F, 0xF9, 0x45, 0xC5, /* .#"._.E. */
	0x2D, 0x96, 0x37, 0x99, 0x88, 0xAD, 0xA2, 0x56, /* -.7....V */
	0x39, 0x74, 0x7E, 0x69,                         /* 9t~i */
};


static const uint8_t mac_data_sha256_key1[24] = {
	'Q', 'W', 'E', 'R', 'T', 'Y'
};

static const uint8_t mac_data_sha256_in1[] = { 'a', 'b', 'c' };

static const uint8_t mac_data_sha256_out1[] = {
	0xee, 0x2e, 0x5d, 0x9b, 0x51, 0xe2, 0x9c, 0x1d,
	0x49, 0xe9, 0xae, 0x6f, 0x0a, 0xcc, 0x15, 0x18,
	0xde, 0x1e, 0xa3, 0x88, 0x8e, 0xee, 0x48, 0xbb,
	0x82, 0x77, 0xe9, 0x09, 0x74, 0x4b, 0xa2, 0xf2
};

/* generated with scripts/digest_hmac.pl */
static const uint8_t mac_data_sha256_key2[24] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_sha256_in2[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_sha256_out2[] = {
	0xF7, 0xBC, 0x83, 0xF4, 0x30, 0x53, 0x84, 0x24, /* ....0S.$ */
	0xB1, 0x32, 0x98, 0xE6, 0xAA, 0x6F, 0xB1, 0x43, /* .2...o.C */
	0xEF, 0x4D, 0x59, 0xA1, 0x49, 0x46, 0x17, 0x59, /* .MY.IF.Y */
	0x97, 0x47, 0x9D, 0xBC, 0x2D, 0x1A, 0x3C, 0xD8, /* .G..-.<. */
};

static const uint8_t mac_data_sha384_key1[32] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_sha384_in1[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_sha384_out1[] = {
	0xD7, 0xF4, 0x72, 0x7E, 0x2C, 0x0B, 0x39, 0xAE, /* ..r~, .9. */
	0x0F, 0x1E, 0x40, 0xCC, 0x96, 0xF6, 0x02, 0x42, /* ..@....B */
	0xD5, 0xB7, 0x80, 0x18, 0x41, 0xCE, 0xA6, 0xFC, /* ....A... */
	0x59, 0x2C, 0x5D, 0x3E, 0x1A, 0xE5, 0x07, 0x00, /* Y, ]>.... */
	0x58, 0x2A, 0x96, 0xCF, 0x35, 0xE1, 0xE5, 0x54, /* X...5..T */
	0x99, 0x5F, 0xE4, 0xE0, 0x33, 0x81, 0xC2, 0x37, /* ._..3..7 */
};

static const uint8_t mac_data_sha512_key1[32] = {
	0x6B, 0x65, 0x79, /* key */
};

static const uint8_t mac_data_sha512_in1[] = {
	0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, /* The quic */
	0x6B, 0x20, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x20, /* k brown  */
	0x66, 0x6F, 0x78, 0x20, 0x6A, 0x75, 0x6D, 0x70, /* fox jump */
	0x73, 0x20, 0x6F, 0x76, 0x65, 0x72, 0x20, 0x74, /* s over t */
	0x68, 0x65, 0x20, 0x6C, 0x61, 0x7A, 0x79, 0x20, /* he lazy  */
	0x64, 0x6F, 0x67,                               /* dog */
};

static const uint8_t mac_data_sha512_out1[] = {
	0xB4, 0x2A, 0xF0, 0x90, 0x57, 0xBA, 0xC1, 0xE2, /* ....W... */
	0xD4, 0x17, 0x08, 0xE4, 0x8A, 0x90, 0x2E, 0x09, /* ........ */
	0xB5, 0xFF, 0x7F, 0x12, 0xAB, 0x42, 0x8A, 0x4F, /* .....B.O */
	0xE8, 0x66, 0x53, 0xC7, 0x3D, 0xD2, 0x48, 0xFB, /* .fS.=.H. */
	0x82, 0xF9, 0x48, 0xA5, 0x49, 0xF7, 0xB7, 0x91, /* ..H.I... */
	0xA5, 0xB4, 0x19, 0x15, 0xEE, 0x4D, 0x1E, 0xC3, /* .....M.. */
	0x93, 0x53, 0x57, 0xE4, 0xE2, 0x31, 0x72, 0x50, /* .SW..1rP */
	0xD0, 0x37, 0x2A, 0xFA, 0x2E, 0xBE, 0xEB, 0x3A, /* .7.....: */
};


/* AES-CBC-MAC */
static const uint8_t mac_cbc_vect1_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static const uint8_t mac_cbc_vect1_data[] = {
	0x43, 0x69, 0x70, 0x68, 0x65, 0x72, 0x20, 0x62, /* Cipher b */
	0x6C, 0x6F, 0x63, 0x6B, 0x20, 0x63, 0x68, 0x61, /* lock cha */
	0x69, 0x6E, 0x69, 0x6E, 0x67, 0x20, 0x28, 0x43, /* ining (C */
	0x42, 0x43, 0x29, 0x20, 0x69, 0x73, 0x20, 0x61, /* BC) is a */
	0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x6F, 0x6E, 0x20, /*  common  */
	0x63, 0x68, 0x61, 0x69, 0x6E, 0x69, 0x6E, 0x67, /* chaining */
	0x20, 0x6D, 0x6F, 0x64, 0x65, 0x20, 0x69, 0x6E, /*  mode in */
	0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x74, /*  which t */
	0x68, 0x65, 0x20, 0x70, 0x72, 0x65, 0x76, 0x69, /* he previ */
	0x6F, 0x75, 0x73, 0x20, 0x62, 0x6C, 0x6F, 0x63, /* ous bloc */
	0x6B, 0x27, 0x73, 0x20, 0x63, 0x69, 0x70, 0x68, /* k's ciph */
	0x65, 0x72, 0x74, 0x65, 0x78, 0x74, 0x20, 0x69, /* ertext i */
	0x73, 0x20, 0x78, 0x6F, 0x72, 0x65, 0x64, 0x20, /* s xored  */
	0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, /* with the */
	0x20, 0x63, 0x75, 0x72, 0x72, 0x65, 0x6E, 0x74, /*  current */
	0x20, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x27, 0x73, /*  block's */
	0x20, 0x70, 0x6C, 0x61, 0x69, 0x6E, 0x74, 0x65, /*  plainte */
	0x78, 0x74, 0x20, 0x62, 0x65, 0x66, 0x6F, 0x72, /* xt befor */
	0x65, 0x20, 0x65, 0x6E, 0x63, 0x72, 0x79, 0x70, /* e encryp */
	0x74, 0x69, 0x6F, 0x6E, 0x2E, 0x2E, 0x2E, 0x2E, /* tion.... */
};

static const uint8_t mac_cbc_vect1_out[] = {
	0xC9, 0x6E, 0x83, 0x7E, 0x35, 0xC8, 0xA7, 0xA0, /* .n.~5... */
	0x33, 0xA3, 0xB1, 0x4B, 0x5A, 0x92, 0x51, 0x2E, /* 3..KZ.Q. */
};

/* DES-CBC-MAC */
static const uint8_t mac_cbc_vect2_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
};

#define mac_cbc_vect2_data mac_cbc_vect1_data
static const uint8_t mac_cbc_vect2_out[] = {
	0xE9, 0x41, 0x46, 0x30, 0x69, 0x32, 0xBD, 0xD6, /* .AF0i2.. */
};

/* DES3-CBC-MAC */
static const uint8_t mac_cbc_vect3_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
};

#define mac_cbc_vect3_data mac_cbc_vect2_data
static const uint8_t mac_cbc_vect3_out[] = {
	0x1C, 0x17, 0xB7, 0xB5, 0x9F, 0x54, 0x9C, 0x63, /* .....T.c */
};

/* AES-CBC-MAC PKCS#5 pad*/
static const uint8_t mac_cbc_vect4_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

#define mac_cbc_vect4_data mac_cbc_vect1_data
static const uint8_t mac_cbc_vect4_out[] = {
	0x0B, 0x46, 0xC7, 0xA2, 0xE1, 0x5A, 0xE2, 0x23, /* .F...Z.# */
	0x83, 0x34, 0x1C, 0x86, 0x53, 0xF8, 0x51, 0x24, /* .4..S.Q$ */
};

/* DES-CBC-MAC PKCS#5 pad*/
static const uint8_t mac_cbc_vect5_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
};

#define mac_cbc_vect5_data mac_cbc_vect1_data
static const uint8_t mac_cbc_vect5_out[] = {
	0x30, 0x81, 0x4F, 0x42, 0x03, 0x7E, 0xD8, 0xA9, /* 0.OB.~.. */
};

/* DES3-CBC-MAC PKCS#5 pad*/
static const uint8_t mac_cbc_vect6_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
};

#define mac_cbc_vect6_data mac_cbc_vect1_data
static const uint8_t mac_cbc_vect6_out[] = {
	0x6E, 0x37, 0x6E, 0x14, 0x5E, 0x21, 0xDD, 0xF8, /* n7n.^!.. */
};

/* AES-CBC-MAC PKCS#5 pad*/
#define mac_cbc_vect7_key mac_cbc_vect4_key
static const uint8_t mac_cbc_vect7_data[] = {
	0x43, 0x69, 0x70, 0x68, 0x65, 0x72, 0x20, 0x62, /* Cipher b */
	0x6C, 0x6F, 0x63, 0x6B, 0x20, 0x63, 0x68, 0x61, /* lock cha */
	0x69, 0x6E, 0x69, 0x6E, 0x67, 0x20, 0x28, 0x43, /* ining (C */
	0x42, 0x43, 0x29, 0x20, 0x69, 0x73, 0x20, 0x61, /* BC) is a */
	0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x6F, 0x6E, 0x20, /*  common  */
	0x63, 0x68, 0x61, 0x69, 0x6E, 0x69, 0x6E, 0x67, /* chaining */
	0x20, 0x6D, 0x6F, 0x64, 0x65, 0x20, 0x69, 0x6E, /*  mode in */
	0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x74, /*  which t */
	0x68, 0x65, 0x20, 0x70, 0x72, 0x65, 0x76, 0x69, /* he previ */
	0x6F, 0x75, 0x73, 0x20, 0x62, 0x6C, 0x6F, 0x63, /* ous bloc */
	0x6B, 0x27, 0x73, 0x20, 0x63, 0x69, 0x70, 0x68, /* k's ciph */
	0x65, 0x72, 0x74, 0x65, 0x78, 0x74, 0x20, 0x69, /* ertext i */
	0x73, 0x20, 0x78, 0x6F, 0x72, 0x65, 0x64, 0x20, /* s xored  */
	0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, /* with the */
	0x20, 0x63, 0x75, 0x72, 0x72, 0x65, 0x6E, 0x74, /*  current */
	0x20, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x27, 0x73, /*  block's */
	0x20, 0x70, 0x6C, 0x61, 0x69, 0x6E, 0x74, 0x65, /*  plainte */
	0x78, 0x74, 0x20, 0x62, 0x65, 0x66, 0x6F, 0x72, /* xt befor */
	0x65, 0x20, 0x65, 0x6E, 0x63, 0x72, 0x79, 0x70, /* e encryp */
	0x74, 0x69, 0x6F, 0x6E, 0x2E,                   /* tion.    */
};

static const uint8_t mac_cbc_vect7_out[] = {
	0xFD, 0x89, 0x35, 0xB3, 0x93, 0x7F, 0xBB, 0xA2, /* ..5..... */
	0xFB, 0x65, 0x60, 0xC4, 0x0A, 0x62, 0xA0, 0xF9, /* .e`..b.. */
};

/* DES-CBC-MAC PKCS#5 pad*/
#define mac_cbc_vect8_key mac_cbc_vect5_key
#define mac_cbc_vect8_data mac_cbc_vect7_data
static const uint8_t mac_cbc_vect8_out[] = {
	0x02, 0x2A, 0xA8, 0x2E, 0x47, 0xC6, 0xBB, 0x7C, /* ....G..| */
};

/* DES3-CBC-MAC PKCS#5 pad*/
#define mac_cbc_vect9_key mac_cbc_vect6_key
#define mac_cbc_vect9_data mac_cbc_vect7_data
static const uint8_t mac_cbc_vect9_out[] = {
	0xD4, 0xF7, 0x3E, 0x27, 0x78, 0x0E, 0x1C, 0x79, /* ..>'x..y */
};

/*
 * DES3-CBC-MAC, with key size of 112bit
 * out obtained with:
 * echo -n "Cipher block chaining (CBC) is a common chaining mode in which the previous block's ciphertext is xored with the current block's plaintext before encryption...."|openssl enc  -iv 0 -des3 -K 303132333435363738394142434445463031323334353637 |xxd
 */
/* DES3-CBC-MAC PKCS#5 pad*/
static const uint8_t mac_cbc_vect10_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};
#define mac_cbc_vect10_data mac_cbc_vect1_data
static const uint8_t mac_cbc_vect10_out[] = {
	0x30, 0x92, 0x60, 0x99, 0x66, 0xac, 0x8c, 0xa6,
};


/*
 * AES-CMAC
 * Test vectors from
 * http://csrc.nist.gov/publications/nistpubs/800-38B/SP_800-38B.pdf
 */

/* AES-128 */
static const uint8_t mac_cmac_vect1_key[] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

static const uint8_t mac_cmac_vect1_out[] = {
	0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
	0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46
};

#define mac_cmac_vect2_key mac_cmac_vect1_key
static const uint8_t mac_cmac_vect2_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};

static const uint8_t mac_cmac_vect2_out[] = {
	0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
	0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c
};

#define mac_cmac_vect3_key mac_cmac_vect1_key
static const uint8_t mac_cmac_vect3_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
};

static const uint8_t mac_cmac_vect3_out[] = {
	0xdf, 0xa6, 0x67, 0x47, 0xde, 0x9a, 0xe6, 0x30,
	0x30, 0xca, 0x32, 0x61, 0x14, 0x97, 0xc8, 0x27
};

#define mac_cmac_vect4_key mac_cmac_vect1_key
static const uint8_t mac_cmac_vect4_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

static const uint8_t mac_cmac_vect4_out[] = {
	0x51, 0xf0, 0xbe, 0xbf, 0x7e, 0x3b, 0x9d, 0x92,
	0xfc, 0x49, 0x74, 0x17, 0x79, 0x36, 0x3c, 0xfe
};

/* AES-192 */
static const uint8_t mac_cmac_vect5_key[] = {
	0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
	0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
	0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
};

static const uint8_t mac_cmac_vect5_out[] = {
	0xd1, 0x7d, 0xdf, 0x46, 0xad, 0xaa, 0xcd, 0xe5,
	0x31, 0xca, 0xc4, 0x83, 0xde, 0x7a, 0x93, 0x67
};


#define mac_cmac_vect6_key mac_cmac_vect5_key
static const uint8_t mac_cmac_vect6_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};

static const uint8_t mac_cmac_vect6_out[] = {
	0x9e, 0x99, 0xa7, 0xbf, 0x31, 0xe7, 0x10, 0x90,
	0x06, 0x62, 0xf6, 0x5e, 0x61, 0x7c, 0x51, 0x84
};

#define mac_cmac_vect7_key mac_cmac_vect5_key
static const uint8_t mac_cmac_vect7_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
};

static const uint8_t mac_cmac_vect7_out[] = {
	0x8a, 0x1d, 0xe5, 0xbe, 0x2e, 0xb3, 0x1a, 0xad,
	0x08, 0x9a, 0x82, 0xe6, 0xee, 0x90, 0x8b, 0x0e
};

#define mac_cmac_vect8_key mac_cmac_vect5_key
static const uint8_t mac_cmac_vect8_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

static const uint8_t mac_cmac_vect8_out[] = {
	0xa1, 0xd5, 0xdf, 0x0e, 0xed, 0x79, 0x0f, 0x79,
	0x4d, 0x77, 0x58, 0x96, 0x59, 0xf3, 0x9a, 0x11
};

/* AES-256 */
static const uint8_t mac_cmac_vect9_key[] = {
	0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
	0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
	0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
	0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};

static const uint8_t mac_cmac_vect9_out[] = {
	0x02, 0x89, 0x62, 0xf6, 0x1b, 0x7b, 0xf8, 0x9e,
	0xfc, 0x6b, 0x55, 0x1f, 0x46, 0x67, 0xd9, 0x83
};

#define mac_cmac_vect10_key mac_cmac_vect9_key
static const uint8_t mac_cmac_vect10_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};

static const uint8_t mac_cmac_vect10_out[] = {
	0x28, 0xa7, 0x02, 0x3f, 0x45, 0x2e, 0x8f, 0x82,
	0xbd, 0x4b, 0xf2, 0x8d, 0x8c, 0x37, 0xc3, 0x5c
};

#define mac_cmac_vect11_key mac_cmac_vect9_key
static const uint8_t mac_cmac_vect11_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
};

static const uint8_t mac_cmac_vect11_out[] = {
	0xaa, 0xf3, 0xd8, 0xf1, 0xde, 0x56, 0x40, 0xc2,
	0x32, 0xf5, 0xb1, 0x69, 0xb9, 0xc9, 0x11, 0xe6
};

#define mac_cmac_vect12_key mac_cmac_vect9_key
static const uint8_t mac_cmac_vect12_data[] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

static const uint8_t mac_cmac_vect12_out[] = {
	0xe1, 0x99, 0x21, 0x90, 0x54, 0x9f, 0x6e, 0xd5,
	0x69, 0x6a, 0x2c, 0x05, 0x6c, 0x31, 0x54, 0x10
};

struct xtest_mac_case {
	uint32_t algo;
	uint32_t key_type;
	const uint8_t *key;
	size_t key_len;
	size_t in_incr;
	const uint8_t *in;
	size_t in_len;
	const uint8_t *out;
	size_t out_len;
	bool multiple_incr;
};

#define XTEST_MAC_CASE_1(algo, key_type, key, in_incr, in, out) \
	{ (algo), (key_type), (key), ARRAY_SIZE(key), \
	  (in_incr), (in), ARRAY_SIZE(in), (out), ARRAY_SIZE(out), false }

#define XTEST_MAC_CASE_MULT(algo, key_type, key, in_incr, in, out) \
	{ (algo), (key_type), (key), ARRAY_SIZE(key), \
	  (in_incr), (in), ARRAY_SIZE(in), (out), ARRAY_SIZE(out), true }

#define XTEST_MAC_CASE(algo, key_type, key, in_incr, in, out) \
	XTEST_MAC_CASE_1((algo), (key_type), (key), (in_incr), (in), (out)), \
	XTEST_MAC_CASE_MULT((algo), (key_type), (key), (in_incr), (in), (out))

#define XTEST_MAC_CBC_CASE(algo, key_type, vect, in_incr) \
	XTEST_MAC_CASE((algo), (key_type), \
		       mac_cbc_ ## vect ## _key, (in_incr), \
		       mac_cbc_ ## vect ## _data, mac_cbc_ ## vect ## _out)

#define XTEST_MAC_CMAC_CASE(vect, in_incr) \
	XTEST_MAC_CASE(TEE_ALG_AES_CMAC, TEE_TYPE_AES, \
		       mac_cmac_ ## vect ## _key, (in_incr), \
		       mac_cmac_ ## vect ## _data, mac_cmac_ ## vect ## _out)

static const struct xtest_mac_case mac_cases[] = {
	XTEST_MAC_CASE(TEE_ALG_HMAC_MD5, TEE_TYPE_HMAC_MD5,
		       mac_data_md5_key1,
		       4, mac_data_md5_in1, mac_data_md5_out1),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA1, TEE_TYPE_HMAC_SHA1,
		       mac_data_sha1_key1,
		       5, mac_data_sha1_in1, mac_data_sha1_out1),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA224, TEE_TYPE_HMAC_SHA224,
		       mac_data_sha224_key1,
		       8, mac_data_sha224_in1, mac_data_sha224_out1),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA256, TEE_TYPE_HMAC_SHA256,
		       mac_data_sha256_key1,
		       1, mac_data_sha256_in1, mac_data_sha256_out1),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA256, TEE_TYPE_HMAC_SHA256,
		       mac_data_sha256_key2,
		       7, mac_data_sha256_in2, mac_data_sha256_out2),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA384, TEE_TYPE_HMAC_SHA384,
		       mac_data_sha384_key1,
		       11, mac_data_sha384_in1, mac_data_sha384_out1),
	XTEST_MAC_CASE(TEE_ALG_HMAC_SHA512, TEE_TYPE_HMAC_SHA512,
		       mac_data_sha512_key1,
		       13, mac_data_sha512_in1, mac_data_sha512_out1),

	XTEST_MAC_CBC_CASE(TEE_ALG_AES_CBC_MAC_NOPAD, TEE_TYPE_AES, vect1, 14),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES_CBC_MAC_NOPAD, TEE_TYPE_DES, vect2, 16),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES3_CBC_MAC_NOPAD, TEE_TYPE_DES3, vect3,
			   17),
	XTEST_MAC_CBC_CASE(TEE_ALG_AES_CBC_MAC_PKCS5, TEE_TYPE_AES, vect4, 11),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES_CBC_MAC_PKCS5, TEE_TYPE_DES, vect5, 9),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES3_CBC_MAC_PKCS5, TEE_TYPE_DES3, vect6, 4),
	XTEST_MAC_CBC_CASE(TEE_ALG_AES_CBC_MAC_PKCS5, TEE_TYPE_AES, vect7, 3),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES_CBC_MAC_PKCS5, TEE_TYPE_DES, vect8, 23),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES3_CBC_MAC_PKCS5, TEE_TYPE_DES3, vect9,
			   34),
	XTEST_MAC_CBC_CASE(TEE_ALG_DES3_CBC_MAC_PKCS5, TEE_TYPE_DES3, vect10, 4),

	{ TEE_ALG_AES_CMAC, TEE_TYPE_AES, mac_cmac_vect1_key,
	  ARRAY_SIZE(mac_cmac_vect1_key), 0, NULL, 0, mac_cmac_vect1_out,
	  ARRAY_SIZE(mac_cmac_vect1_out) },
	XTEST_MAC_CMAC_CASE(vect2, 9),
	XTEST_MAC_CMAC_CASE(vect3, 9),
	XTEST_MAC_CMAC_CASE(vect4, 9),
	{ TEE_ALG_AES_CMAC, TEE_TYPE_AES, mac_cmac_vect5_key,
	  ARRAY_SIZE(mac_cmac_vect5_key), 0, NULL, 0, mac_cmac_vect5_out,
	  ARRAY_SIZE(mac_cmac_vect5_out) },
	XTEST_MAC_CMAC_CASE(vect6, 9),
	XTEST_MAC_CMAC_CASE(vect7, 9),
	XTEST_MAC_CMAC_CASE(vect8, 9),
	{ TEE_ALG_AES_CMAC, TEE_TYPE_AES, mac_cmac_vect9_key,
	  ARRAY_SIZE(mac_cmac_vect9_key), 0, NULL, 0, mac_cmac_vect9_out,
	  ARRAY_SIZE(mac_cmac_vect9_out) },
	XTEST_MAC_CMAC_CASE(vect10, 9),
	XTEST_MAC_CMAC_CASE(vect11, 9),
	XTEST_MAC_CMAC_CASE(vect12, 9),
};

static void xtest_tee_test_4002(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	TEE_OperationHandle op1;
	TEE_OperationHandle op2;
	TEE_ObjectHandle key_handle;
	uint8_t out[64];
	size_t out_size;
	uint32_t ret_orig;
	size_t n;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(mac_cases); n++) {
		TEE_Attribute key_attr;
		size_t key_size;
		size_t offs;

		Do_ADBG_BeginSubCase(c, "MAC case %d algo 0x%x",
				     (int)n, (unsigned int)mac_cases[n].algo);

		key_attr.attributeID = TEE_ATTR_SECRET_VALUE;
		key_attr.content.ref.buffer = (void *)mac_cases[n].key;
		key_attr.content.ref.length = mac_cases[n].key_len;

		key_size = key_attr.content.ref.length * 8;
		if (mac_cases[n].key_type == TEE_TYPE_DES ||
		    mac_cases[n].key_type == TEE_TYPE_DES3)
			/* Exclude parity in bit size of key */
			key_size -= key_size / 8;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op1,
				mac_cases[n].algo, TEE_MODE_MAC, key_size)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op2,
				mac_cases[n].algo, TEE_MODE_MAC, key_size)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_transient_object(c, &session,
				mac_cases[n].key_type, key_size, &key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_populate_transient_object(c, &session,
				key_handle, &key_attr, 1)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_set_operation_key(c, &session, op1,
				key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_mac_init(c, &session, op1, NULL, 0)))
			goto out;

		offs = 0;
		if (mac_cases[n].in != NULL) {
			while (offs + mac_cases[n].in_incr <
					mac_cases[n].in_len) {
				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_mac_update(c, &session,
						op1, mac_cases[n].in + offs,
						mac_cases[n].in_incr)))
					goto out;
				offs += mac_cases[n].in_incr;
				if (!mac_cases[n].multiple_incr)
					break;
			}
		}

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_copy_operation(c, &session, op2, op1)))
			goto out;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_mac_final_compute(c, &session, op2,
				mac_cases[n].in + offs,
				mac_cases [n].in_len - offs,
				out, &out_size)))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, mac_cases[n].out,
					 mac_cases[n].out_len, out, out_size);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_mac_init(c, &session, op1, NULL, 0)))
			goto out;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_mac_final_compute(c, &session, op1,
				mac_cases[n].in, mac_cases[n].in_len, out,
				&out_size)))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, mac_cases[n].out,
					 mac_cases[n].out_len, out, out_size);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op1)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op2)))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
	}
out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4002, xtest_tee_test_4002,
		"Test TEE Internal API MAC operations");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence in implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4211(ADBG_Case_t *c, CK_SLOT_ID slot);

/* AES CMAC test resrouces */
#define CK_MAC_KEY_AES(_key_array) \
	{								\
		{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_VERIFY, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_AES},		\
						sizeof(CK_KEY_TYPE) },	\
		{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},	\
						sizeof(CK_OBJECT_CLASS) }, \
		{ CKA_VALUE, (void *)_key_array, sizeof(_key_array) }, \
	}

static CK_ATTRIBUTE cktest_aes_cmac_key1[] =
	CK_MAC_KEY_AES(mac_cmac_vect1_key);

static CK_ATTRIBUTE cktest_aes_cmac_key2[] =
	CK_MAC_KEY_AES(mac_cmac_vect5_key);

static CK_ATTRIBUTE cktest_aes_cmac_key3[] =
	CK_MAC_KEY_AES(mac_cmac_vect9_key);

static CK_MECHANISM cktest_aes_cmac_mechanism = {
	CKM_AES_CMAC, NULL, 0,
};
#if 0
static CK_MECHANISM cktest_aes_cmac_gen_mechanism = {
	CKM_AES_CMAC_GENERAL, &(CK_ULONG){12}, sizeof(CK_ULONG),
};
#endif

/* HMAC test resrouces */

#define CK_MAC_KEY_HMAC(_type, _key_array) \
	{								\
		{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },	\
		{ CKA_VERIFY, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){_type}, sizeof(CK_KEY_TYPE) }, \
		{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},	\
						sizeof(CK_OBJECT_CLASS) }, \
		{ CKA_VALUE, (void *)_key_array, sizeof(_key_array) },	\
	}

static CK_ATTRIBUTE cktest_hmac_md5_key[] =
	CK_MAC_KEY_HMAC(CKK_MD5_HMAC, mac_data_md5_key1);

static CK_ATTRIBUTE cktest_hmac_sha1_key[] =
	CK_MAC_KEY_HMAC(CKK_SHA_1_HMAC, mac_data_sha1_key1);

static CK_ATTRIBUTE cktest_hmac_sha224_key[] =
	CK_MAC_KEY_HMAC(CKK_SHA224_HMAC, mac_data_sha224_key1);

static CK_ATTRIBUTE cktest_hmac_sha256_key1[] =
	CK_MAC_KEY_HMAC(CKK_SHA256_HMAC, mac_data_sha256_key1);

static CK_ATTRIBUTE cktest_hmac_sha256_key2[] =
	CK_MAC_KEY_HMAC(CKK_SHA256_HMAC, mac_data_sha256_key2);

static CK_ATTRIBUTE cktest_hmac_sha384_key[] =
	CK_MAC_KEY_HMAC(CKK_SHA384_HMAC, mac_data_sha384_key1);

static CK_ATTRIBUTE cktest_hmac_sha512_key[] =
	CK_MAC_KEY_HMAC(CKK_SHA512_HMAC, mac_data_sha512_key1);

static CK_MECHANISM cktest_hmac_md5_mechanism = {
	CKM_MD5_HMAC, NULL, 0,
};
static CK_MECHANISM cktest_hmac_sha1_mechanism = {
	CKM_SHA_1_HMAC, NULL, 0,
};
static CK_MECHANISM cktest_hmac_sha224_mechanism = {
	CKM_SHA224_HMAC, NULL, 0,
};
static CK_MECHANISM cktest_hmac_sha256_mechanism = {
	CKM_SHA256_HMAC, NULL, 0,
};
static CK_MECHANISM cktest_hmac_sha384_mechanism = {
	CKM_SHA384_HMAC, NULL, 0,
};
static CK_MECHANISM cktest_hmac_sha512_mechanism = {
	CKM_SHA512_HMAC, NULL, 0,
};

/* AES CBC MAC test resrouces */

static CK_ATTRIBUTE cktest_aes_cbc_mac_key1[] =
	CK_MAC_KEY_AES(mac_cbc_vect1_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key2[] =
	CK_MAC_KEY_AES(mac_cbc_vect2_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key3[] =
	CK_MAC_KEY_AES(mac_cbc_vect3_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key4[] =
	CK_MAC_KEY_AES(mac_cbc_vect4_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key5[] =
	CK_MAC_KEY_AES(mac_cbc_vect5_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key6[] =
	CK_MAC_KEY_AES(mac_cbc_vect6_key);

static CK_ATTRIBUTE cktest_aes_cbc_mac_key10[] =
	CK_MAC_KEY_AES(mac_cbc_vect10_key);

static CK_MECHANISM cktest_aes_cbc_mac_mechanism = {
	CKM_AES_XCBC_MAC, NULL, 0,
};

static int get_ck_mac_case(size_t mac_case_index, CK_MECHANISM_PTR *mechanism,
			   CK_ATTRIBUTE_PTR *key, CK_ULONG *count)
{
	const struct xtest_mac_case *mac_case = &mac_cases[mac_case_index];

	*mechanism = NULL;
	*key = NULL;
	*count = 0;

	switch (mac_case->algo) {
	case TEE_ALG_AES_CMAC:
		*mechanism = &cktest_aes_cmac_mechanism;
		break;
	case TEE_ALG_HMAC_MD5:
		*mechanism = &cktest_hmac_md5_mechanism;
		break;
	case TEE_ALG_HMAC_SHA1:
		*mechanism = &cktest_hmac_sha1_mechanism;
		break;
	case TEE_ALG_HMAC_SHA224:
		*mechanism = &cktest_hmac_sha224_mechanism;
		break;
	case TEE_ALG_HMAC_SHA256:
		*mechanism = &cktest_hmac_sha256_mechanism;
		break;
	case TEE_ALG_HMAC_SHA384:
		*mechanism = &cktest_hmac_sha384_mechanism;
		break;
	case TEE_ALG_HMAC_SHA512:
		*mechanism = &cktest_hmac_sha512_mechanism;
		break;
	case TEE_ALG_AES_CBC_MAC_NOPAD:
		*mechanism = &cktest_aes_cbc_mac_mechanism;
		break;
	default:
		break;
	}

	/* AES CMAC key attributes reference (key and attributes count */
	if (mac_case->key == mac_cmac_vect1_key) {
		*key = cktest_aes_cmac_key1;
		*count = ARRAY_SIZE(cktest_aes_cmac_key1);
	}
	if (mac_case->key == mac_cmac_vect5_key) {
		*key = cktest_aes_cmac_key2;
		*count = ARRAY_SIZE(cktest_aes_cmac_key2);
	}
	if (mac_case->key == mac_cmac_vect9_key) {
		*key = cktest_aes_cmac_key3;
		*count = ARRAY_SIZE(cktest_aes_cmac_key3);
	}
	/* HMAC key attributes reference (key and attributes count */
	if (mac_case->key == mac_data_md5_key1) {
		*key = cktest_hmac_md5_key;
		*count = ARRAY_SIZE(cktest_hmac_md5_key);
	}
	if (mac_case->key == mac_data_sha1_key1) {
		*key = cktest_hmac_sha1_key;
		*count = ARRAY_SIZE(cktest_hmac_sha1_key);
	}
	if (mac_case->key == mac_data_sha224_key1) {
		*key = cktest_hmac_sha224_key;
		*count = ARRAY_SIZE(cktest_hmac_sha224_key);
	}
	if (mac_case->key == mac_data_sha256_key1) {
		*key = cktest_hmac_sha256_key1;
		*count = ARRAY_SIZE(cktest_hmac_sha256_key1);
	}
	if (mac_case->key == mac_data_sha256_key2) {
		*key = cktest_hmac_sha256_key2;
		*count = ARRAY_SIZE(cktest_hmac_sha256_key2);
	}
	if (mac_case->key == mac_data_sha384_key1) {
		*key = cktest_hmac_sha384_key;
		*count = ARRAY_SIZE(cktest_hmac_sha384_key);
	}
	if (mac_case->key == mac_data_sha512_key1) {
		*key = cktest_hmac_sha512_key;
		*count = ARRAY_SIZE(cktest_hmac_sha512_key);
	}
	/* AES CBC MAC key attributes reference (key and attributes count */
	if (mac_case->key == mac_cbc_vect1_key) {
		*key = cktest_aes_cbc_mac_key1;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key1);
	}
	if (mac_case->key == mac_cbc_vect2_key) {
		*key = cktest_aes_cbc_mac_key2;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key2);
	}
	if (mac_case->key == mac_cbc_vect3_key) {
		*key = cktest_aes_cbc_mac_key3;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key3);
	}
	if (mac_case->key == mac_cbc_vect4_key) {
		*key = cktest_aes_cbc_mac_key4;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key4);
	}
	if (mac_case->key == mac_cbc_vect5_key) {
		*key = cktest_aes_cbc_mac_key5;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key5);
	}
	if (mac_case->key == mac_cbc_vect6_key) {
		*key = cktest_aes_cbc_mac_key6;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key6);
	}
	if (mac_case->key == mac_cbc_vect10_key) {
		*key = cktest_aes_cbc_mac_key10;
		*count = ARRAY_SIZE(cktest_aes_cbc_mac_key10);
	}

	if (!*mechanism || !*key || !*count)
		return 1;

	return 0;
}

void run_xtest_tee_test_4211(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	CK_RV rv;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE key_handle;
	uint8_t out[64];
	CK_ULONG out_size;
	size_t n;
	int close_subcase = 0;
	struct xtest_mac_case const *test;

	rv = C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
			   NULL, 0, &session);
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto out;

	for (n = 0; n < ARRAY_SIZE(mac_cases); n++) {
		CK_ATTRIBUTE_PTR ck_key = NULL;
		CK_MECHANISM_PTR mechanism = NULL;
		CK_ULONG attr_count = 0;

		if (get_ck_mac_case(n, &mechanism, &ck_key, &attr_count)) {
			Do_ADBG_Log("Skip case %lu algo 0x%x",
				    n, (unsigned int)mac_cases[n].algo);
			continue;
		}

		Do_ADBG_BeginSubCase(c, "MAC case %lu algo 0x%x (%s)",
				     n, (unsigned int)mac_cases[n].algo,
				     ckm2str(mechanism->mechanism));

		close_subcase = 1;
		test = &mac_cases[n];

		rv = C_CreateObject(session, ck_key, attr_count, &key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		/* Test 1 shot signature */
		if (mac_cases[n].in != NULL) {
			rv = C_SignInit(session, mechanism, key_handle);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			rv = C_SignUpdate(session,
					  (void *)test->in, test->in_len);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			/* Test too short buffer case */
			out_size = 1;
			rv = C_SignFinal(session, out, &out_size);

			if (!ADBG_EXPECT_CK_RESULT(c, rv, CKR_BUFFER_TOO_SMALL))
				goto out;

			/* Get to full output */
			out_size = sizeof(out);
			memset(out, 0, sizeof(out));
			rv = C_SignFinal(session, out, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c, mac_cases[n].out,
						 mac_cases[n].out_len,
						 out, out_size);
		}

		/* Test 2 step update signature */
		rv = C_SignInit(session, mechanism, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		if (mac_cases[n].in != NULL) {
			rv = C_SignUpdate(session,
					  (void *)test->in, test->in_incr);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			rv = C_SignUpdate(session,
					 (void *)(test->in + test->in_incr),
					 test->in_len - test->in_incr);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;
		}

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));

		rv = C_SignFinal(session, out, &out_size);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		(void)ADBG_EXPECT_BUFFER(c, mac_cases[n].out,
					 mac_cases[n].out_len, out, out_size);

		rv = C_DestroyObject(session, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
		close_subcase = 0;
	}
out:
	if (close_subcase)
		Do_ADBG_EndSubCase(c, NULL);

	rv = C_CloseSession(session);
	ADBG_EXPECT_CK_OK(c, rv);
}
#endif

static const uint8_t ciph_data_aes_key1[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static const uint8_t ciph_data_aes_key2[] = {
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
};

static const uint8_t ciph_data_des_key1[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 /* 01234567 */
};

static const uint8_t ciph_data_des_key2[] = {
	0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1
};


static const uint8_t ciph_data_des3_key1[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
};

static const uint8_t ciph_data_des3_key2[] = {
	0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1,
	0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1,
	0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1
};

static const uint8_t ciph_data_des2_key1[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static const uint8_t ciph_data_in1[] = {
	0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, /* 23456789 */
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, /* ABCDEF01 */
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, /* 3456789A */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, 0x32, /* BCDEF012 */
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, /* 456789AB */
	0x43, 0x44, 0x45, 0x46, 0x30, 0x31, 0x32, 0x33, /* CDEF0123 */
};

static const uint8_t ciph_data_in3[] = {
	0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, /* 23456789 */
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, /* ABCDEF01 */
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, /* 3456789A */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, 0x32, /* BCDEF012 */
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, /* 456789AB */
	0x43, 0x44, 0x45, 0x46, 0x30,                   /* CDEF0    */
};

static const uint8_t ciph_data_in4[] = {
	0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, /* 23456789 */
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, /* ABCDEF01 */
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, /* 3456789A */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x30, 0x31, 0x32, /* BCDEF012 */
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, /* 456789AB */
	0x43, 0x44, 0x45, 0x46, 0x30, 0x31, 0x32, 0x33, /* CDEF0123 */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static const uint8_t ciph_data_in5[] = {
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01
};

static const uint8_t ciph_data_128_iv1[] = {
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
	0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x30, /* 9ABCDEF0 */
};

static const uint8_t ciph_data_128_iv2[] = {
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
};

static const uint8_t ciph_data_64_iv1[] = {
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
};

static const uint8_t ciph_data_in2[] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
};

static const uint8_t ciph_data_aes_ecb_nopad_out1[] = {
	0xA5, 0xBE, 0x14, 0xD5, 0x01, 0x05, 0x24, 0x48, /* ......$H */
	0x58, 0x1A, 0x6B, 0x82, 0xD4, 0x41, 0xD2, 0xB3, /* X.k..A.. */
	0xBB, 0xF9, 0xAE, 0x37, 0x94, 0xAC, 0x18, 0x50, /* ...7...P */
	0x87, 0x09, 0xCC, 0x3F, 0x43, 0xD2, 0xC2, 0xB7, /* ...?C... */
	0xD7, 0x6F, 0x84, 0x07, 0xB4, 0x78, 0xCE, 0x34, /* .o...x.4 */
	0x48, 0xC9, 0x16, 0x86, 0x42, 0xB8, 0xFF, 0xCF, /* H...B... */
};

static const uint8_t ciph_data_aes_cbc_nopad_out1[] = {
	0x8D, 0x9F, 0x88, 0xD8, 0xAF, 0x9F, 0xC1, 0x3B, /* .......; */
	0x02, 0x15, 0x43, 0x6A, 0x8C, 0x1E, 0x34, 0x5C, /* ..Cj..4\ */
	0x83, 0xF4, 0x85, 0x3E, 0x43, 0x0F, 0xE5, 0x5F, /* ...>C.._ */
	0x81, 0x4C, 0xC0, 0x28, 0x3F, 0xD9, 0x98, 0x53, /* .L.(?..S */
	0xB1, 0x44, 0x51, 0x38, 0x21, 0xAB, 0x10, 0xCE, /* .DQ8!... */
	0xC2, 0xEC, 0x65, 0x54, 0xDD, 0x5C, 0xEA, 0xDC, /* ..eT.\.. */
};

static const uint8_t ciph_data_aes_ctr_out1[] = {
	0xD2, 0xDD, 0x11, 0xA8, 0xF7, 0xB0, 0xAE, 0x55, /* .......U */
	0xBE, 0x61, 0x7A, 0xE6, 0xA1, 0x6C, 0x79, 0xF4, /* .az..ly. */
	0x62, 0x51, 0x7B, 0xE9, 0x7C, 0xA0, 0x31, 0x0C, /* bQ{.|.1. */
	0x24, 0x15, 0x70, 0x7F, 0x47, 0x37, 0x69, 0xE0, /* $.p.G7i. */
	0x24, 0xC3, 0x29, 0xCD, 0xF2, 0x26, 0x69, 0xFF, /* $.)..&i. */
	0x72, 0x0E, 0x3C, 0xD1, 0xA1, 0x2F, 0x5D, 0x33, /* r.<../]3 */
};

static const uint8_t ciph_data_aes_ctr_out2[] = {
	0xD2, 0xDD, 0x11, 0xA8, 0xF7, 0xB0, 0xAE, 0x55, /* .......U */
	0xBE, 0x61, 0x7A, 0xE6, 0xA1, 0x6C, 0x79, 0xF4, /* .az..ly. */
	0x62, 0x51, 0x7B, 0xE9, 0x7C, 0xA0, 0x31, 0x0C, /* bQ{.|.1. */
	0x24, 0x15, 0x70, 0x7F, 0x47, 0x37, 0x69, 0xE0, /* $.p.G7i. */
	0x24, 0xC3, 0x29, 0xCD, 0xF2, 0x26, 0x69, 0xFF, /* $.)..&i. */
	0x72, 0x0E, 0x3C, 0xD1, 0xA1,                   /* r.<..    */
};

static const uint8_t ciph_data_aes_ctr_out4[] = {
	0xD2, 0xDD, 0x11, 0xA8, 0xF7, 0xB0, 0xAE, 0x55, /* .......U */
	0xBE, 0x61, 0x7A, 0xE6, 0xA1, 0x6C, 0x79, 0xF4, /* .az..ly. */
	0x62, 0x51, 0x7B, 0xE9, 0x7C, 0xA0, 0x31, 0x0C, /* bQ{.|.1. */
	0x24, 0x15, 0x70, 0x7F, 0x47, 0x37, 0x69, 0xE0, /* $.p.G7i. */
	0x24, 0xC3, 0x29, 0xCD, 0xF2, 0x26, 0x69, 0xFF, /* $.)..&i. */
	0x72, 0x0E, 0x3C, 0xD1, 0xA1, 0x2F, 0x5D, 0x33, /* r.<../]3 */
	0x9F, 0xD7, 0x0C, 0x92, 0xD4, 0xA5, 0x9D, 0x06, /* ........ */
	0x01, 0x80, 0x38, 0xCD, 0xC2, 0x71, 0x5D, 0x4A, /* ..8..q]J */
};

static const uint8_t ciph_data_aes_ctr_out5[] = {
	0xbb, 0xfe, 0x07, 0x04, 0x1c, 0x8e, 0x09, 0x61,
	0xfb, 0xb1, 0x7c, 0xa5, 0x4d, 0x2b, 0x30, 0xf6,
	0x26, 0x9e, 0xff, 0x61, 0x18, 0x47, 0xc6, 0x06,
	0x81, 0x02, 0x84, 0xcd, 0x9c, 0x4b, 0x6d, 0x21,
	0xe2, 0x64, 0xa6, 0x50, 0x7f, 0x28, 0x81, 0x6f,
	0x29, 0xda, 0xd5, 0x56, 0x3f, 0x46, 0xac, 0xca,
	0x37, 0xe7, 0x77, 0x36, 0xbc, 0x76, 0x39, 0x57,
	0xaa, 0x67, 0x1b, 0x2a, 0xe6, 0x36, 0x57, 0x6d,
	0x2a, 0xb8, 0x77, 0x41, 0xc2, 0x4e, 0x4f, 0x27,
	0x4c, 0x34, 0x7a, 0x01, 0x6a, 0xda, 0x75, 0x75,
	0x3e, 0x68, 0xb2
};

static const uint8_t ciph_data_aes_cbc_vect1_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static const uint8_t ciph_data_aes_cbc_vect1_iv[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
};

static const uint8_t ciph_data_aes_cbc_vect1_ptx[] = {
	0x43, 0x69, 0x70, 0x68, 0x65, 0x72, 0x20, 0x62, /* Cipher b */
	0x6C, 0x6F, 0x63, 0x6B, 0x20, 0x63, 0x68, 0x61, /* lock cha */
	0x69, 0x6E, 0x69, 0x6E, 0x67, 0x20, 0x28, 0x43, /* ining (C */
	0x42, 0x43, 0x29, 0x20, 0x69, 0x73, 0x20, 0x61, /* BC) is a */
	0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x6F, 0x6E, 0x20, /*  common  */
	0x63, 0x68, 0x61, 0x69, 0x6E, 0x69, 0x6E, 0x67, /* chaining */
	0x20, 0x6D, 0x6F, 0x64, 0x65, 0x20, 0x69, 0x6E, /*  mode in */
	0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x74, /*  which t */
	0x68, 0x65, 0x20, 0x70, 0x72, 0x65, 0x76, 0x69, /* he previ */
	0x6F, 0x75, 0x73, 0x20, 0x62, 0x6C, 0x6F, 0x63, /* ous bloc */
	0x6B, 0x27, 0x73, 0x20, 0x63, 0x69, 0x70, 0x68, /* k's ciph */
	0x65, 0x72, 0x74, 0x65, 0x78, 0x74, 0x20, 0x69, /* ertext i */
	0x73, 0x20, 0x78, 0x6F, 0x72, 0x65, 0x64, 0x20, /* s xored  */
	0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, /* with the */
	0x20, 0x63, 0x75, 0x72, 0x72, 0x65, 0x6E, 0x74, /*  current */
	0x20, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x27, 0x73, /*  block's */
	0x20, 0x70, 0x6C, 0x61, 0x69, 0x6E, 0x74, 0x65, /*  plainte */
	0x78, 0x74, 0x20, 0x62, 0x65, 0x66, 0x6F, 0x72, /* xt befor */
	0x65, 0x20, 0x65, 0x6E, 0x63, 0x72, 0x79, 0x70, /* e encryp */
	0x74, 0x69, 0x6F, 0x6E, 0x2E, 0x2E, 0x2E, 0x2E, /* tion.... */
};

static const uint8_t ciph_data_aes_cbc_vect1_ctx[] = {
	0xDD, 0x16, 0xC3, 0x47, 0x69, 0xEC, 0xF5, 0x86, /* ...Gi... */
	0x20, 0xB4, 0xF7, 0xE3, 0xB9, 0x36, 0xE8, 0xAC, /*  ....6.. */
	0xC9, 0xA2, 0x11, 0x71, 0x3B, 0x10, 0x9D, 0x45, /* ...q;..E */
	0xCA, 0xE2, 0x49, 0xA6, 0x4E, 0x56, 0x07, 0xC5, /* ..I.NV.. */
	0xCE, 0xA3, 0x14, 0xB5, 0x30, 0x1B, 0x63, 0xBB, /* ....0.c. */
	0x2D, 0x6F, 0xE9, 0xCA, 0x0B, 0x00, 0x31, 0x3F, /* -o....1? */
	0xA4, 0x9A, 0x39, 0xE1, 0xC8, 0xD2, 0x24, 0x95, /* ..9...$. */
	0x14, 0xE9, 0xD2, 0x43, 0xE8, 0xA8, 0x1E, 0x0A, /* ...C.... */
	0xFE, 0x9D, 0x4F, 0xF5, 0xBB, 0x16, 0xB9, 0x54, /* ..O....T */
	0x78, 0x07, 0x94, 0x05, 0x8E, 0x47, 0xC3, 0xCB, /* x....G.. */
	0x7C, 0xEC, 0xF4, 0xF8, 0xF2, 0xA4, 0x59, 0x6E, /* |.....Yn */
	0xED, 0xAD, 0x7F, 0x62, 0xAF, 0x89, 0xA8, 0x5B, /* ...b...[ */
	0x75, 0xD4, 0x73, 0xE3, 0xBA, 0x9F, 0x9A, 0xD2, /* u.s..... */
	0x0F, 0xFD, 0x3C, 0xE6, 0xC6, 0xA4, 0xD6, 0x6C, /* ..<....l */
	0x6A, 0x09, 0xE2, 0x16, 0xB0, 0x8C, 0x69, 0x3C, /* j.....i< */
	0xC8, 0x1C, 0xE4, 0x3E, 0x86, 0x4D, 0xB0, 0x2B, /* ...>.M.+ */
	0x29, 0xA0, 0x5A, 0xA3, 0x67, 0xBA, 0xDC, 0x11, /* ).Z.g... */
	0x08, 0x5E, 0x69, 0xB4, 0x6F, 0xA5, 0xE2, 0xB8, /* .^i.o... */
	0xC9, 0x6E, 0x83, 0x7E, 0x35, 0xC8, 0xA7, 0xA0, /* .n.~5... */
	0x33, 0xA3, 0xB1, 0x4B, 0x5A, 0x92, 0x51, 0x2E, /* 3..KZ.Q. */
};

/* AES-CTS test vectors from http://tools.ietf.org/html/rfc3962
 * and http://tools.ietf.org/html/draft-raeburn-krb-rijndael-krb-02 */
static const uint8_t ciph_data_aes_cts_vect1_key[] = {
	0x63, 0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x20,
	0x74, 0x65, 0x72, 0x69, 0x79, 0x61, 0x6b, 0x69
};

static const uint8_t ciph_data_aes_cts_vect1_iv[16] = {
	0x00
};

static const uint8_t ciph_data_aes_cts_vect1_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20
};

static const uint8_t ciph_data_aes_cts_vect1_ctx[] = {
	0xc6, 0x35, 0x35, 0x68, 0xf2, 0xbf, 0x8c, 0xb4,
	0xd8, 0xa5, 0x80, 0x36, 0x2d, 0xa7, 0xff, 0x7f,
	0x97
};

#define ciph_data_aes_cts_vect2_key ciph_data_aes_cts_vect1_key
#define ciph_data_aes_cts_vect2_iv ciph_data_aes_cts_vect1_iv
static const uint8_t ciph_data_aes_cts_vect2_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x6c,
	0x20, 0x47, 0x61, 0x75, 0x27, 0x73, 0x20
};

static const uint8_t ciph_data_aes_cts_vect2_ctx[] = {
	0xfc, 0x00, 0x78, 0x3e, 0x0e, 0xfd, 0xb2, 0xc1,
	0xd4, 0x45, 0xd4, 0xc8, 0xef, 0xf7, 0xed, 0x22,
	0x97, 0x68, 0x72, 0x68, 0xd6, 0xec, 0xcc, 0xc0,
	0xc0, 0x7b, 0x25, 0xe2, 0x5e, 0xcf, 0xe5
};

#define ciph_data_aes_cts_vect3_key ciph_data_aes_cts_vect1_key
#define ciph_data_aes_cts_vect3_iv ciph_data_aes_cts_vect1_iv
static const uint8_t ciph_data_aes_cts_vect3_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x6c,
	0x20, 0x47, 0x61, 0x75, 0x27, 0x73, 0x20, 0x43,
};

static const uint8_t ciph_data_aes_cts_vect3_ctx[] = {
	0x39, 0x31, 0x25, 0x23, 0xa7, 0x86, 0x62, 0xd5,
	0xbe, 0x7f, 0xcb, 0xcc, 0x98, 0xeb, 0xf5, 0xa8,
	0x97, 0x68, 0x72, 0x68, 0xd6, 0xec, 0xcc, 0xc0,
	0xc0, 0x7b, 0x25, 0xe2, 0x5e, 0xcf, 0xe5, 0x84,
};

#define ciph_data_aes_cts_vect4_key ciph_data_aes_cts_vect1_key
#define ciph_data_aes_cts_vect4_iv ciph_data_aes_cts_vect1_iv
static const uint8_t ciph_data_aes_cts_vect4_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x6c,
	0x20, 0x47, 0x61, 0x75, 0x27, 0x73, 0x20, 0x43,
	0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x2c, 0x20,
	0x70, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x2c,
};

static const uint8_t ciph_data_aes_cts_vect4_ctx[] = {
	0x97, 0x68, 0x72, 0x68, 0xd6, 0xec, 0xcc, 0xc0,
	0xc0, 0x7b, 0x25, 0xe2, 0x5e, 0xcf, 0xe5, 0x84,
	0xb3, 0xff, 0xfd, 0x94, 0x0c, 0x16, 0xa1, 0x8c,
	0x1b, 0x55, 0x49, 0xd2, 0xf8, 0x38, 0x02, 0x9e,
	0x39, 0x31, 0x25, 0x23, 0xa7, 0x86, 0x62, 0xd5,
	0xbe, 0x7f, 0xcb, 0xcc, 0x98, 0xeb, 0xf5,
};

#define ciph_data_aes_cts_vect5_key ciph_data_aes_cts_vect1_key
#define ciph_data_aes_cts_vect5_iv ciph_data_aes_cts_vect1_iv
static const uint8_t ciph_data_aes_cts_vect5_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x6c,
	0x20, 0x47, 0x61, 0x75, 0x27, 0x73, 0x20, 0x43,
	0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x2c, 0x20,
	0x70, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x2c, 0x20,
};

static const uint8_t ciph_data_aes_cts_vect5_ctx[] = {
	0x97, 0x68, 0x72, 0x68, 0xd6, 0xec, 0xcc, 0xc0,
	0xc0, 0x7b, 0x25, 0xe2, 0x5e, 0xcf, 0xe5, 0x84,
	0x9d, 0xad, 0x8b, 0xbb, 0x96, 0xc4, 0xcd, 0xc0,
	0x3b, 0xc1, 0x03, 0xe1, 0xa1, 0x94, 0xbb, 0xd8,
	0x39, 0x31, 0x25, 0x23, 0xa7, 0x86, 0x62, 0xd5,
	0xbe, 0x7f, 0xcb, 0xcc, 0x98, 0xeb, 0xf5, 0xa8,
};

#define ciph_data_aes_cts_vect6_key ciph_data_aes_cts_vect1_key
#define ciph_data_aes_cts_vect6_iv ciph_data_aes_cts_vect1_iv
static const uint8_t ciph_data_aes_cts_vect6_ptx[] = {
	0x49, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20,
	0x6c, 0x69, 0x6b, 0x65, 0x20, 0x74, 0x68, 0x65,
	0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x6c,
	0x20, 0x47, 0x61, 0x75, 0x27, 0x73, 0x20, 0x43,
	0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x2c, 0x20,
	0x70, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x2c, 0x20,
	0x61, 0x6e, 0x64, 0x20, 0x77, 0x6f, 0x6e, 0x74,
	0x6f, 0x6e, 0x20, 0x73, 0x6f, 0x75, 0x70, 0x2e,
};

static const uint8_t ciph_data_aes_cts_vect6_ctx[] = {
	0x97, 0x68, 0x72, 0x68, 0xd6, 0xec, 0xcc, 0xc0,
	0xc0, 0x7b, 0x25, 0xe2, 0x5e, 0xcf, 0xe5, 0x84,
	0x39, 0x31, 0x25, 0x23, 0xa7, 0x86, 0x62, 0xd5,
	0xbe, 0x7f, 0xcb, 0xcc, 0x98, 0xeb, 0xf5, 0xa8,
	0x48, 0x07, 0xef, 0xe8, 0x36, 0xee, 0x89, 0xa5,
	0x26, 0x73, 0x0d, 0xbc, 0x2f, 0x7b, 0xc8, 0x40,
	0x9d, 0xad, 0x8b, 0xbb, 0x96, 0xc4, 0xcd, 0xc0,
	0x3b, 0xc1, 0x03, 0xe1, 0xa1, 0x94, 0xbb, 0xd8,
};

/*
 * Test case for https://github.com/OP-TEE/optee_os/issues/1203
 * 80 bytes of data, processed in two steps (32 + 48).
 */

#define ciph_data_aes_cts_issue1203_key ciph_data_aes_cts_vect1_key

static const uint8_t ciph_data_aes_cts_issue1203_iv[] = {
	0x5b, 0x11, 0x7b, 0xf9, 0x83, 0x6a, 0x01, 0x27,
	0x1d, 0x2a, 0xae, 0x0a, 0xd3, 0x27, 0x35, 0x81,
};

static const uint8_t ciph_data_aes_cts_issue1203_ptx[] = {
	0x33, 0x2a, 0xf4, 0x8c, 0x18, 0xa1, 0x30, 0x7c,
	0xa7, 0x52, 0x76, 0xca, 0xa6, 0x68, 0xad, 0xf6,
	0x1e, 0xba, 0xc9, 0x66, 0x66, 0x52, 0x1a, 0xe7,
	0x3e, 0xbc, 0xba, 0x60, 0x95, 0x71, 0x04, 0x5e,
	0xb4, 0xb7, 0x10, 0xd7, 0x93, 0x6d, 0x5f, 0x9a,
	0xb8, 0xbb, 0x6b, 0xb0, 0x6d, 0x47, 0x93, 0x41,
	0x29, 0x21, 0xf2, 0x84, 0xf5, 0xb2, 0xf2, 0x6d,
	0xbe, 0xe5, 0x8a, 0x2b, 0x58, 0x8d, 0x1b, 0x5e,
	0x41, 0x04, 0xba, 0x9e, 0x92, 0xe9, 0x52, 0xb6,
	0x8b, 0x1e, 0x36, 0x72, 0x91, 0xe4, 0x56, 0x66,
};

/*
 * Ciphertext was generated by an online tool for AES CBC.
 * Since the input size is a multiple of the block size, and the ciphertext
 * format is CS3, the output is the same as plain AES CBC with the last
 * two blocks swapped.
 */
static const uint8_t ciph_data_aes_cts_issue1203_ctx[] = {
	0x6c, 0xfe, 0xef, 0x6a, 0x1c, 0x8f, 0xd3, 0xc3,
	0x66, 0xc4, 0xd9, 0x0e, 0xbe, 0x24, 0x17, 0x3f,
	0xcd, 0x19, 0xc9, 0xd2, 0x10, 0x3f, 0xcf, 0xdc,
	0xe9, 0x05, 0xab, 0xd5, 0x1a, 0x31, 0x41, 0x77,
	0x3b, 0xd7, 0x6a, 0x26, 0xae, 0x0d, 0xc9, 0x47,
	0x77, 0xd4, 0xa7, 0xd5, 0x73, 0x88, 0x9e, 0xde,
	0x72, 0x66, 0xe0, 0xe2, 0x3d, 0x62, 0x91, 0x22,
	0xae, 0x11, 0x01, 0x97, 0x22, 0x6a, 0x45, 0xa9,
	0xba, 0x56, 0xd2, 0xbc, 0x55, 0x2d, 0x99, 0x4d,
	0x3d, 0x22, 0x27, 0x9a, 0xac, 0x66, 0x2f, 0xf5,
};

static const uint8_t ciph_data_des_ecb_nopad_out1[] = {
	0x46, 0x2B, 0x91, 0xA8, 0x55, 0xE6, 0x7E, 0x75, /* F+..U.~u */
	0x5E, 0x53, 0xF4, 0x8F, 0x29, 0x41, 0x4E, 0xEF, /* ^S..)AN. */
	0x32, 0x1B, 0x58, 0x42, 0x9B, 0xB4, 0x3A, 0x1F, /* 2.XB..:. */
	0x9A, 0xEA, 0xA4, 0xB4, 0xCD, 0xE9, 0x87, 0x7C, /* .......| */
	0xC0, 0x05, 0x34, 0xED, 0x86, 0x3C, 0x2A, 0x81, /* ..4..<.. */
	0x5E, 0x93, 0x0E, 0x86, 0xA9, 0xBB, 0x80, 0xFF, /* ^....... */
};

static const uint8_t ciph_data_des_ecb_nopad_out2[] = {
	0x85, 0xE8, 0x13, 0x54, 0x0F, 0x0A, 0xB4, 0x05
};

static const uint8_t ciph_data_des_cbc_nopad_out1[] = {
	0xAD, 0xD6, 0xD6, 0x3E, 0x88, 0xD0, 0xDD, 0xD4, /* ...>.... */
	0x9A, 0x93, 0x95, 0xA0, 0x86, 0x22, 0x5B, 0x9E, /* ....."[. */
	0x84, 0x0C, 0x6F, 0x12, 0x04, 0x11, 0x6F, 0xD4, /* ..o...o. */
	0x12, 0x29, 0xC6, 0x78, 0x0C, 0xFB, 0x58, 0x5B, /* .).x..X[ */
	0x66, 0x82, 0x0E, 0x52, 0xDC, 0x25, 0x16, 0x51, /* f..R.%.Q */
	0x69, 0xDF, 0xFE, 0x4E, 0x11, 0x1D, 0x9D, 0x08, /* i..N.... */
};

static const uint8_t ciph_data_des3_ecb_nopad_out1[] = {
	0xA7, 0x46, 0xEC, 0xA8, 0x6A, 0x7F, 0x4A, 0xF1, /* .F..j.J. */
	0x44, 0x60, 0x37, 0x3B, 0x7F, 0x76, 0xE5, 0xFF, /* D`7;.v.. */
	0xC1, 0xE7, 0xA5, 0x04, 0x9C, 0x29, 0x5C, 0xA5, /* .....)\. */
	0xDD, 0xC8, 0xEE, 0x36, 0x1F, 0x48, 0xE0, 0xA9, /* ...6.H.. */
	0x82, 0x2D, 0x53, 0x84, 0x49, 0x69, 0x78, 0xFA, /* .-S.Iix. */
	0x23, 0x69, 0x1D, 0xF7, 0x08, 0x38, 0x44, 0x05, /* #i...8D. */
};

static const uint8_t ciph_data_des3_cbc_nopad_out1[] = {
	0x18, 0x09, 0xEB, 0x98, 0xE4, 0x58, 0x72, 0x11, /* .....Xr. */
	0x30, 0x58, 0xAB, 0x38, 0xB0, 0xC9, 0x2B, 0xED, /* 0X.8..+. */
	0xDA, 0xC5, 0xE8, 0xA9, 0xF6, 0x8A, 0xA7, 0x80, /* ........ */
	0xBE, 0x54, 0x1F, 0x63, 0xF6, 0xEE, 0xA2, 0x4C, /* .T.c...L */
	0x7C, 0xEB, 0x84, 0x7D, 0xDA, 0xCA, 0x1E, 0xB2, /* |..}.... */
	0xED, 0x5E, 0x96, 0xB8, 0x01, 0x4B, 0x77, 0x02, /* .^...Kw. */
};

static const uint8_t ciph_data_des2_ecb_nopad_out1[] = {
	0xAB, 0x12, 0xB6, 0xE2, 0x4A, 0x3A, 0x26, 0x14, /* ....J:&. */
	0xF0, 0x7D, 0x23, 0xD0, 0x55, 0xDF, 0x5C, 0x16, /* .}#.U.\. */
	0x43, 0x59, 0x1E, 0x44, 0x01, 0x76, 0xD7, 0x50, /* CY.D.v.P */
	0x44, 0xC0, 0x15, 0xDF, 0x2E, 0x7F, 0x8B, 0xC5, /* D....... */
	0xFF, 0x8B, 0x87, 0xFE, 0x33, 0xD7, 0xCB, 0x2C, /* ....3.., */
	0xDA, 0x79, 0x6F, 0xA4, 0x05, 0x2B, 0x30, 0xCE, /* .yo..+0. */
};

static const uint8_t ciph_data_des2_cbc_nopad_out1[] = {
	0x47, 0x2F, 0xB1, 0x83, 0xC4, 0xBB, 0x93, 0x16, /* G/...... */
	0x73, 0xF9, 0xAD, 0x6F, 0x00, 0xF9, 0xCB, 0x4A, /* s..o...J */
	0x0F, 0x4F, 0x75, 0x75, 0xFB, 0x39, 0x0B, 0xFC, /* .Ouu.9.. */
	0x9F, 0x48, 0x52, 0xAD, 0xA2, 0x75, 0x2C, 0xF1, /* .HR..u, . */
	0x7D, 0xC3, 0x8F, 0x16, 0xCF, 0xC9, 0x76, 0x29, /* }.....v) */
	0x1A, 0xBF, 0xB3, 0xD9, 0x10, 0x7E, 0xAA, 0x49, /* .....~.I */
};

struct xtest_ciph_case {
	uint32_t algo;
	uint32_t mode;
	uint32_t key_type;
	const uint8_t *key1;
	size_t key1_len;
	const uint8_t *key2;
	size_t key2_len;
	const uint8_t *iv;
	size_t iv_len;
	size_t in_incr;
	const uint8_t *in;
	size_t in_len;
	const uint8_t *out;
	size_t out_len;
	size_t line;
};

#define XTEST_CIPH_CASE_NO_IV(algo, key_type, key, in_incr, ptx, ctx) \
	{ (algo), TEE_MODE_ENCRYPT, (key_type), (key), ARRAY_SIZE(key), \
	  NULL, 0, NULL, 0, \
	  (in_incr), (ptx), ARRAY_SIZE(ptx), (ctx), ARRAY_SIZE(ctx), \
	  __LINE__ }, \
	{ (algo), TEE_MODE_DECRYPT, (key_type), (key), ARRAY_SIZE(key), \
	  NULL, 0, NULL, 0, \
	  (in_incr), (ctx), ARRAY_SIZE(ctx), (ptx), ARRAY_SIZE(ptx), __LINE__ }

#define XTEST_CIPH_CASE(algo, key_type, key, iv, in_incr, ptx, ctx) \
	{ (algo), TEE_MODE_ENCRYPT, (key_type), (key), ARRAY_SIZE(key), \
	  NULL, 0, iv, ARRAY_SIZE(iv), (in_incr), (ptx), ARRAY_SIZE(ptx), \
	  (ctx), ARRAY_SIZE(ctx), __LINE__ }, \
	{ (algo), TEE_MODE_DECRYPT, (key_type), (key), ARRAY_SIZE(key), \
	  NULL, 0, iv, ARRAY_SIZE(iv), (in_incr), (ctx), ARRAY_SIZE(ctx),  \
	  (ptx), ARRAY_SIZE(ptx), __LINE__ }

#define XTEST_CIPH_CASE_AES_XTS(vect, in_incr) \
	{ TEE_ALG_AES_XTS, TEE_MODE_ENCRYPT, TEE_TYPE_AES, \
	  ciph_data_aes_xts_ ## vect ## _key1, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _key1), \
	  ciph_data_aes_xts_ ## vect ## _key2, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _key2), \
	  ciph_data_aes_xts_ ## vect ## _iv, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _iv), \
	  (in_incr), \
	  ciph_data_aes_xts_ ## vect ## _ptx, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _ptx), \
	  ciph_data_aes_xts_ ## vect ## _ctx, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _ctx), __LINE__ }, \
	{ TEE_ALG_AES_XTS, TEE_MODE_DECRYPT, TEE_TYPE_AES, \
	  ciph_data_aes_xts_ ## vect ## _key1, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _key1), \
	  ciph_data_aes_xts_ ## vect ## _key2, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _key2), \
	  ciph_data_aes_xts_ ## vect ## _iv, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _iv), \
	  (in_incr), \
	  ciph_data_aes_xts_ ## vect ## _ctx, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _ctx), \
	  ciph_data_aes_xts_ ## vect ## _ptx, \
	  ARRAY_SIZE(ciph_data_aes_xts_ ## vect ## _ptx), __LINE__ }

#define XTEST_CIPH_CASE_AES_CBC(vect, in_incr) \
	XTEST_CIPH_CASE(TEE_ALG_AES_CBC_NOPAD, TEE_TYPE_AES, \
			ciph_data_aes_cbc_ ## vect ## _key, \
			ciph_data_aes_cbc_ ## vect ## _iv, \
			(in_incr), ciph_data_aes_cbc_ ## vect ## _ptx, \
			ciph_data_aes_cbc_ ## vect ## _ctx)

#define XTEST_CIPH_CASE_AES_CTS(vect, in_incr) \
	XTEST_CIPH_CASE(TEE_ALG_AES_CTS, TEE_TYPE_AES, \
			ciph_data_aes_cts_ ## vect ## _key, \
			ciph_data_aes_cts_ ## vect ## _iv, \
			(in_incr), ciph_data_aes_cts_ ## vect ## _ptx, \
			ciph_data_aes_cts_ ## vect ## _ctx)

static const struct xtest_ciph_case ciph_cases[] = {
	/* AES */
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_AES_ECB_NOPAD, TEE_TYPE_AES,
			      ciph_data_aes_key1, 11, ciph_data_in1,
			      ciph_data_aes_ecb_nopad_out1),
	XTEST_CIPH_CASE(TEE_ALG_AES_CBC_NOPAD, TEE_TYPE_AES,
			ciph_data_aes_key1, ciph_data_128_iv1, 11,
			ciph_data_in1,
			ciph_data_aes_cbc_nopad_out1),
	XTEST_CIPH_CASE(TEE_ALG_AES_CTR, TEE_TYPE_AES,
			ciph_data_aes_key1, ciph_data_128_iv1, 13,
			ciph_data_in1,
			ciph_data_aes_ctr_out1),
	XTEST_CIPH_CASE(TEE_ALG_AES_CTR, TEE_TYPE_AES,
			ciph_data_aes_key1, ciph_data_128_iv1, 13,
			ciph_data_in3,
			ciph_data_aes_ctr_out2),
	XTEST_CIPH_CASE(TEE_ALG_AES_CTR, TEE_TYPE_AES,
			ciph_data_aes_key1, ciph_data_128_iv1, 16,
			ciph_data_in3,
			ciph_data_aes_ctr_out2),
	XTEST_CIPH_CASE(TEE_ALG_AES_CTR, TEE_TYPE_AES,
			ciph_data_aes_key1, ciph_data_128_iv1, 16,
			ciph_data_in4,
			ciph_data_aes_ctr_out4),
	XTEST_CIPH_CASE(TEE_ALG_AES_CTR, TEE_TYPE_AES,
			ciph_data_aes_key2, ciph_data_128_iv2, 11,
			ciph_data_in5,
			ciph_data_aes_ctr_out5),

	XTEST_CIPH_CASE_AES_CBC(vect1, 11),
	XTEST_CIPH_CASE_AES_CBC(vect1, 64),

	/* AES-CTS */
	XTEST_CIPH_CASE_AES_CTS(vect1, 13),
	XTEST_CIPH_CASE_AES_CTS(vect2, 14),
	XTEST_CIPH_CASE_AES_CTS(vect3, 11),
	XTEST_CIPH_CASE_AES_CTS(vect4, 9),
	XTEST_CIPH_CASE_AES_CTS(vect5, 7),
	XTEST_CIPH_CASE_AES_CTS(vect6, 17),
	XTEST_CIPH_CASE_AES_CTS(vect6, 0x20),
	XTEST_CIPH_CASE_AES_CTS(issue1203, 32),

	/* DES */
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_DES_ECB_NOPAD, TEE_TYPE_DES,
			      ciph_data_des_key1, 14, ciph_data_in1,
			      ciph_data_des_ecb_nopad_out1),
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_DES_ECB_NOPAD, TEE_TYPE_DES,
			      ciph_data_des_key2, 3, ciph_data_in2,
			      ciph_data_des_ecb_nopad_out2),
	XTEST_CIPH_CASE(TEE_ALG_DES_CBC_NOPAD, TEE_TYPE_DES,
			ciph_data_des_key1, ciph_data_64_iv1, 15, ciph_data_in1,
			ciph_data_des_cbc_nopad_out1),

	/* DES3 */
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_DES3_ECB_NOPAD, TEE_TYPE_DES3,
			      ciph_data_des3_key1, 11, ciph_data_in1,
			      ciph_data_des3_ecb_nopad_out1),
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_DES3_ECB_NOPAD, TEE_TYPE_DES3,
			      ciph_data_des3_key2, 3, ciph_data_in2,
			      ciph_data_des_ecb_nopad_out2),
	XTEST_CIPH_CASE(TEE_ALG_DES3_CBC_NOPAD, TEE_TYPE_DES3,
			ciph_data_des3_key1, ciph_data_64_iv1, 11,
			ciph_data_in1,
			ciph_data_des3_cbc_nopad_out1),

	/* DES2 */
	XTEST_CIPH_CASE_NO_IV(TEE_ALG_DES3_ECB_NOPAD, TEE_TYPE_DES3,
			      ciph_data_des2_key1, 11, ciph_data_in1,
			      ciph_data_des2_ecb_nopad_out1),
	XTEST_CIPH_CASE(TEE_ALG_DES3_CBC_NOPAD, TEE_TYPE_DES3,
			ciph_data_des2_key1, ciph_data_64_iv1, 11,
			ciph_data_in1,
			ciph_data_des2_cbc_nopad_out1),

	/* AES-XTS */
	XTEST_CIPH_CASE_AES_XTS(vect1, 3),
	XTEST_CIPH_CASE_AES_XTS(vect2, 6),
	XTEST_CIPH_CASE_AES_XTS(vect3, 7),
	XTEST_CIPH_CASE_AES_XTS(vect4, 8),
	XTEST_CIPH_CASE_AES_XTS(vect5, 9),
	XTEST_CIPH_CASE_AES_XTS(vect6, 13),
	XTEST_CIPH_CASE_AES_XTS(vect7, 1),
	XTEST_CIPH_CASE_AES_XTS(vect8, 3),
	XTEST_CIPH_CASE_AES_XTS(vect9, 2),
	XTEST_CIPH_CASE_AES_XTS(vect10, 5),
	XTEST_CIPH_CASE_AES_XTS(vect11, 6),
	XTEST_CIPH_CASE_AES_XTS(vect12, 7),
	XTEST_CIPH_CASE_AES_XTS(vect13, 3),
	XTEST_CIPH_CASE_AES_XTS(vect14, 2),
	XTEST_CIPH_CASE_AES_XTS(vect15, 0),
	XTEST_CIPH_CASE_AES_XTS(vect16, 9),
	XTEST_CIPH_CASE_AES_XTS(vect17, 6),
	XTEST_CIPH_CASE_AES_XTS(vect18, 8),
	XTEST_CIPH_CASE_AES_XTS(vect19, 23),
};

static void xtest_tee_test_4003(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	TEE_OperationHandle op;
	TEE_ObjectHandle key1_handle = TEE_HANDLE_NULL;
	TEE_ObjectHandle key2_handle = TEE_HANDLE_NULL;
	uint8_t out[2048];
	size_t out_size;
	size_t out_offs;
	uint32_t ret_orig;
	size_t n;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(ciph_cases); n++) {
		TEE_Attribute key_attr;
		size_t key_size;
		size_t op_key_size;


		Do_ADBG_BeginSubCase(c, "Cipher case %d algo 0x%x line %d",
				     (int)n, (unsigned int)ciph_cases[n].algo,
				     (int)ciph_cases[n].line);

		key_attr.attributeID = TEE_ATTR_SECRET_VALUE;
		key_attr.content.ref.buffer = (void *)ciph_cases[n].key1;
		key_attr.content.ref.length = ciph_cases[n].key1_len;

		key_size = key_attr.content.ref.length * 8;
		if (ciph_cases[n].key_type == TEE_TYPE_DES ||
		    ciph_cases[n].key_type == TEE_TYPE_DES3)
			/* Exclude parity in bit size of key */
			key_size -= key_size / 8;

		op_key_size = key_size;
		if (ciph_cases[n].key2 != NULL)
			op_key_size *= 2;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op,
				ciph_cases[n].algo, ciph_cases[n].mode,
				op_key_size)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_transient_object(c, &session,
				ciph_cases[n].key_type, key_size,
				&key1_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_populate_transient_object(c, &session,
				key1_handle, &key_attr, 1)))
			goto out;

		if (ciph_cases[n].key2 != NULL) {
			key_attr.content.ref.buffer =
				(void *)ciph_cases[n].key2;
			key_attr.content.ref.length = ciph_cases[n].key2_len;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_transient_object(c,
					&session, ciph_cases[n].key_type,
					key_attr.content.ref.length * 8,
					&key2_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_populate_transient_object(c,
					&session, key2_handle, &key_attr, 1)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key2(c, &session, op,
					key1_handle, key2_handle)))
				goto out;
		} else {
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					key1_handle)))
				goto out;
		}

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				key1_handle)))
			goto out;
		key1_handle = TEE_HANDLE_NULL;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				key2_handle)))
			goto out;
		key2_handle = TEE_HANDLE_NULL;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_cipher_init(c, &session, op,
				ciph_cases[n].iv, ciph_cases[n].iv_len)))
			goto out;

		out_offs = 0;
		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_cipher_update(c, &session, op,
				ciph_cases[n].in, ciph_cases[n].in_incr, out,
				&out_size)))
			goto out;

		if (ciph_cases[n].algo == TEE_ALG_AES_CTR)
			ADBG_EXPECT_COMPARE_UNSIGNED(c, out_size, ==,
				ciph_cases[n].in_incr);

		out_offs += out_size;
		out_size = sizeof(out) - out_offs;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_cipher_do_final(c, &session, op,
				ciph_cases[n].in + ciph_cases[n].in_incr,
				ciph_cases[n].in_len - ciph_cases[n].in_incr,
				out + out_offs,
				&out_size)))
			goto out;

		out_offs += out_size;

		(void)ADBG_EXPECT_BUFFER(c, ciph_cases[n].out,
					 ciph_cases[n].out_len, out, out_offs);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op)))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
	}
out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4003, xtest_tee_test_4003,
		"Test TEE Internal API cipher operations");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence in implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4210(ADBG_Case_t *c, CK_SLOT_ID slot);

#define CK_CIPHERING_KEY_AES(_key_array) \
	{								\
		{ CKA_ENCRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_AES},		\
						sizeof(CK_KEY_TYPE) },  \
		{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},	\
					    sizeof(CK_OBJECT_CLASS) }, \
		{ CKA_VALUE, (void *)_key_array, sizeof(_key_array) }, \
	}

static CK_ATTRIBUTE cktest_aes_flavours_key1[] =
	CK_CIPHERING_KEY_AES(ciph_data_aes_key1);

static CK_ATTRIBUTE cktest_aes_flavours_key2[] =
	CK_CIPHERING_KEY_AES(ciph_data_aes_cbc_vect1_key);

static CK_ATTRIBUTE cktest_aes_flavours_key3[] =
	CK_CIPHERING_KEY_AES(ciph_data_aes_cts_vect1_key);

static CK_ATTRIBUTE cktest_aes_flavours_key4[] =
	CK_CIPHERING_KEY_AES(ciph_data_aes_key2);

/* This is a dump of ciph_data_128_iv1, as CK expects it in a structure */
static CK_AES_CTR_PARAMS cktest_aes_ctr_params1 = {
	.ulCounterBits = 1,
	.cb = {
		0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* 12345678 */
		0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x30, /* 9ABCDEF0 */
	},
};

/* This is a dump of ciph_data_128_iv2, as CK expects it in a structure */
static CK_AES_CTR_PARAMS cktest_aes_ctr_params2 = {
	.ulCounterBits = 1,
	.cb = {
		0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
		0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	},
};

static CK_MECHANISM cktest_aes_ecb_mechanism = {
	CKM_AES_ECB, NULL, 0,
};
static CK_MECHANISM cktest_aes_cbc_mechanism1 = {
	CKM_AES_CBC, (CK_BYTE_PTR)ciph_data_128_iv1,
	sizeof(ciph_data_128_iv1),
};
static CK_MECHANISM cktest_aes_cbc_mechanism2 = {
	CKM_AES_CBC, (CK_BYTE_PTR)ciph_data_aes_cbc_vect1_iv,
	sizeof(ciph_data_aes_cbc_vect1_iv),
};
static CK_MECHANISM cktest_aes_ctr_mechanism1 = {
	CKM_AES_CTR, (CK_BYTE_PTR)&cktest_aes_ctr_params1,
	sizeof(cktest_aes_ctr_params1),
};
static CK_MECHANISM cktest_aes_ctr_mechanism2 = {
	CKM_AES_CTR, (CK_BYTE_PTR)&cktest_aes_ctr_params2,
	sizeof(cktest_aes_ctr_params2),
};
static CK_MECHANISM cktest_aes_cts_mechanism1 = {
	CKM_AES_CTS, (CK_BYTE_PTR)ciph_data_aes_cts_vect1_iv,
	sizeof(ciph_data_aes_cts_vect1_iv),
};
static CK_MECHANISM cktest_aes_cts_mechanism2 = {
	CKM_AES_CTS, (CK_BYTE_PTR)ciph_data_aes_cts_issue1203_iv,
	sizeof(ciph_data_aes_cts_issue1203_iv),
};

static int get_ck_ciph_case(size_t ciph_case_index, CK_MECHANISM_PTR *mechanism,
			    CK_ATTRIBUTE_PTR *key, CK_ULONG *count)
{
	const struct xtest_ciph_case *ciph_case = &ciph_cases[ciph_case_index];

	*mechanism = NULL;
	*key = NULL;
	*count = 0;

	switch (ciph_case->algo) {
	case TEE_ALG_AES_ECB_NOPAD:
		*mechanism = &cktest_aes_ecb_mechanism;
		break;
	case TEE_ALG_AES_CBC_NOPAD:
		if (ciph_case->iv == ciph_data_128_iv1) {
			*mechanism = &cktest_aes_cbc_mechanism1;
		}
		if (ciph_case->iv == ciph_data_aes_cbc_vect1_iv) {
			*mechanism = &cktest_aes_cbc_mechanism2;
		}
		break;
	case TEE_ALG_AES_CTS:
		if (ciph_case->iv == ciph_data_aes_cts_vect1_iv) {
			*mechanism = &cktest_aes_cts_mechanism1;
		}
		if (ciph_case->iv == ciph_data_aes_cts_issue1203_iv) {
			*mechanism = &cktest_aes_cts_mechanism2;
		}
		break;
	case TEE_ALG_AES_CTR:
		if (ciph_case->iv == ciph_data_128_iv1)
			*mechanism = &cktest_aes_ctr_mechanism1;
		if (ciph_case->iv == ciph_data_128_iv2)
			*mechanism = &cktest_aes_ctr_mechanism2;
		break;
	default:
		break;
	}

	if (ciph_case->key1 == ciph_data_aes_key1) {
		*key = cktest_aes_flavours_key1;
		*count = ARRAY_SIZE(cktest_aes_flavours_key1);
	}
	if (ciph_case->key1 == ciph_data_aes_key2) {
		*key = cktest_aes_flavours_key4;
		*count = ARRAY_SIZE(cktest_aes_flavours_key4);
	}
	if (ciph_case->key1 == ciph_data_aes_cbc_vect1_key) {
		*key = cktest_aes_flavours_key2;
		*count = ARRAY_SIZE(cktest_aes_flavours_key2);
	}
	if (ciph_case->key1 == ciph_data_aes_cts_vect1_key) {
		*key = cktest_aes_flavours_key3;
		*count = ARRAY_SIZE(cktest_aes_flavours_key3);
	}

	if (!*mechanism || !*key || !*count)
		return 1;

	return 0;
}

void run_xtest_tee_test_4210(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	CK_RV rv;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE key_handle;
	uint8_t out[2048];
	CK_ULONG out_size;
	CK_ULONG out_offs;
	size_t n;
	int close_subcase = 0;

	rv = C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
			   NULL, 0, &session);
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto out;

	for (n = 0; n < ARRAY_SIZE(ciph_cases); n++) {
		CK_ATTRIBUTE_PTR ck_key = NULL;
		CK_MECHANISM_PTR mechanism = NULL;
		CK_ULONG attr_count = 0;

		if (get_ck_ciph_case(n, &mechanism, &ck_key, &attr_count)) {
			Do_ADBG_Log("Skip case %lu algo 0x%x line %u",
				    n, (unsigned int)ciph_cases[n].algo,
				    (unsigned int)ciph_cases[n].line);
			continue;
		}

		Do_ADBG_BeginSubCase(c, "Cipher case %lu algo 0x%x (%s) line %u",
					n, (unsigned int)ciph_cases[n].algo,
					ckm2str(mechanism->mechanism),
					(unsigned int)ciph_cases[n].line);
		close_subcase = 1;

		rv = C_CreateObject(session, ck_key, attr_count, &key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		if (ciph_cases[n].mode == TEE_MODE_ENCRYPT)
			rv = C_EncryptInit(session, mechanism, key_handle);

		if (ciph_cases[n].mode == TEE_MODE_DECRYPT)
			rv = C_DecryptInit(session, mechanism, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		out_offs = 0;
		out_size = sizeof(out);
		memset(out, 0, sizeof(out));

		if (ciph_cases[n].mode == TEE_MODE_ENCRYPT)
			rv = C_EncryptUpdate(session,
					     (void *)ciph_cases[n].in,
					     ciph_cases[n].in_incr,
					     out, &out_size);

		if (ciph_cases[n].mode == TEE_MODE_DECRYPT)
			rv = C_DecryptUpdate(session,
					     (void *)ciph_cases[n].in,
					     ciph_cases[n].in_incr,
					     out, &out_size);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;


		if (ciph_cases[n].algo == TEE_ALG_AES_CTR)
			ADBG_EXPECT_COMPARE_UNSIGNED(c, out_size, ==,
				ciph_cases[n].in_incr);

		out_offs += out_size;
		out_size = sizeof(out) - out_offs;

		if (ciph_cases[n].mode == TEE_MODE_ENCRYPT)
			rv = C_EncryptUpdate(session,
				(void *)(ciph_cases[n].in +
					 ciph_cases[n].in_incr),
				ciph_cases[n].in_len - ciph_cases[n].in_incr,
				out + out_offs, &out_size);

		if (ciph_cases[n].mode == TEE_MODE_DECRYPT)
			rv = C_DecryptUpdate(session,
				(void *)(ciph_cases[n].in +
					 ciph_cases[n].in_incr),
				ciph_cases[n].in_len - ciph_cases[n].in_incr,
				out + out_offs, &out_size);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		out_offs += out_size;
		out_size = sizeof(out) - out_offs;

		if (ciph_cases[n].mode == TEE_MODE_ENCRYPT)
			rv = C_EncryptFinal(session, out + out_offs, &out_size);

		if (ciph_cases[n].mode == TEE_MODE_DECRYPT)
			rv = C_DecryptFinal(session, out + out_offs,  &out_size);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		out_offs += out_size;

		(void)ADBG_EXPECT_BUFFER(c, ciph_cases[n].out,
					 ciph_cases[n].out_len, out, out_offs);

		rv = C_DestroyObject(session, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
		close_subcase = 0;
	}
out:
	if (close_subcase)
		Do_ADBG_EndSubCase(c, NULL);

	rv = C_CloseSession(session);
	ADBG_EXPECT_CK_OK(c, rv);
}
#endif

static void xtest_tee_test_4004(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	uint8_t buf1[45] = { 0 };
	uint8_t buf2[45] = { 0 };
	static const uint8_t zeros[45] = { 0 };

	Do_ADBG_BeginSubCase(c, "TEE get random");
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
			&ret_orig)))
		return;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_random_number_generate(c, &session, buf1,
			sizeof(buf1))))
		goto out;

	(void)ADBG_EXPECT_COMPARE_SIGNED(c,
		0, !=, memcmp(buf1, zeros, sizeof(buf1)));

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_random_number_generate(c, &session, buf2,
			sizeof(buf2))))
		goto out;

	(void)ADBG_EXPECT_COMPARE_SIGNED(c,
		0, !=, memcmp(buf2, zeros, sizeof(buf2)));

	(void)ADBG_EXPECT_COMPARE_SIGNED(c,
		0, !=, memcmp(buf2, buf1, sizeof(buf1)));
out:
	TEEC_CloseSession(&session);
	Do_ADBG_EndSubCase(c, "TEE get random");
}
ADBG_CASE_DEFINE(regression, 4004, xtest_tee_test_4004,
		"Test TEE Internal API get random");

struct xtest_ae_case {
	uint32_t algo;
	uint32_t mode;
	uint32_t key_type;
	const uint8_t *key;
	size_t key_len;
	const uint8_t *nonce;
	size_t nonce_len;
	size_t aad_incr;
	const uint8_t *aad;
	size_t aad_len;
	size_t in_incr;
	const uint8_t *ptx;
	size_t ptx_len;
	const uint8_t *ctx;
	size_t ctx_len;
	const uint8_t *tag;
	size_t tag_len;
	size_t line;
};


#define ARRAY(a)            a, ARRAY_SIZE(a)
#define NULL_ARRAY(a)       NULL, 0

#define XTEST_AE_CASE(algo, vect, aad_incr, in_incr, \
		      aad_array, ptx_array, ctx_array) \
	{ (algo), TEE_MODE_ENCRYPT, TEE_TYPE_AES, ARRAY(vect ## _key), \
	  ARRAY(vect ## _nonce), (aad_incr), \
		aad_array(vect ## _aad), (in_incr), \
	  ptx_array(vect ## _ptx), ctx_array(vect ## _ctx), \
		ARRAY(vect ## _tag), \
	  __LINE__ }, \
	{ (algo), TEE_MODE_DECRYPT, TEE_TYPE_AES, ARRAY(vect ## _key), \
	  ARRAY(vect ## _nonce), (aad_incr), \
		aad_array(vect ## _aad), (in_incr), \
	  ptx_array(vect ## _ptx), ctx_array(vect ## _ctx), \
		ARRAY(vect ## _tag), \
	  __LINE__ }

#define XTEST_AE_CASE_AES_CCM(vect, aad_incr, in_incr) \
	XTEST_AE_CASE(TEE_ALG_AES_CCM, ae_data_aes_ccm_ ## vect, aad_incr, \
		      in_incr, ARRAY, ARRAY, ARRAY)

#define XTEST_AE_CASE_AES_GCM(vect, aad_incr, in_incr, \
			      aad_array, ptx_array, ctx_array) \
	XTEST_AE_CASE(TEE_ALG_AES_GCM, ae_data_aes_gcm_ ## vect, aad_incr, \
		      in_incr, aad_array, ptx_array, ctx_array)



static const struct xtest_ae_case ae_cases[] = {
	XTEST_AE_CASE_AES_CCM(vect1, 3, 2),
	XTEST_AE_CASE_AES_CCM(vect2, 7, 13),
	XTEST_AE_CASE_AES_CCM(vect3, 5, 21),

	XTEST_AE_CASE_AES_GCM(vect1, 0, 0, NULL_ARRAY, NULL_ARRAY, NULL_ARRAY),
	XTEST_AE_CASE_AES_GCM(vect2, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect3, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect3, 0, 0x1F, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect4, 5, 0x20, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect5, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect6, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect7, 0, 0, NULL_ARRAY, NULL_ARRAY, NULL_ARRAY),
	XTEST_AE_CASE_AES_GCM(vect8, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect9, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect10, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect11, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect12, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect13, 0, 0, NULL_ARRAY, NULL_ARRAY, NULL_ARRAY),
	XTEST_AE_CASE_AES_GCM(vect14, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect15, 0, 9, NULL_ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect16, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect17, 5, 9, ARRAY, ARRAY, ARRAY),
	XTEST_AE_CASE_AES_GCM(vect18, 5, 9, ARRAY, ARRAY, ARRAY),
#ifdef CFG_GCM_NIST_VECTORS
#include "gcmDecrypt128.h"
#include "gcmDecrypt192.h"
#include "gcmDecrypt256.h"
#include "gcmEncryptExtIV128.h"
#include "gcmEncryptExtIV192.h"
#include "gcmEncryptExtIV256.h"
#endif
};

static void xtest_tee_test_4005(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	TEE_OperationHandle op;
	TEE_ObjectHandle key_handle = TEE_HANDLE_NULL;
	TEE_Attribute key_attr;
	uint8_t out[512];
	size_t out_size;
	size_t out_offs;
	uint32_t ret_orig;
	size_t n;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
			&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(ae_cases); n++) {
		Do_ADBG_BeginSubCase(c, "AE case %d algo 0x%x line %d",
				     (int)n, (unsigned int)ae_cases[n].algo,
				     (int)ae_cases[n].line);

		key_attr.attributeID = TEE_ATTR_SECRET_VALUE;
		key_attr.content.ref.buffer = (void *)ae_cases[n].key;
		key_attr.content.ref.length = ae_cases[n].key_len;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op,
				ae_cases[n].algo, ae_cases[n].mode,
				key_attr.content.ref.length * 8)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_transient_object(c, &session,
				ae_cases[n].key_type,
				key_attr.content.ref.length * 8,
				&key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_populate_transient_object(c, &session,
				key_handle, &key_attr, 1)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_set_operation_key(c, &session, op,
				key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				key_handle)))
			goto out;
		key_handle = TEE_HANDLE_NULL;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_ae_init(c, &session, op, ae_cases[n].nonce,
			ae_cases[n].nonce_len, ae_cases[n].tag_len,
			ae_cases[n].aad_len, ae_cases[n].ptx_len)))
			goto out;

		if (ae_cases[n].aad != NULL) {
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_ae_update_aad(c, &session, op,
					ae_cases[n].aad, ae_cases[n].aad_incr)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_ae_update_aad(c, &session, op,
					ae_cases[n].aad + ae_cases[n].aad_incr,
					ae_cases [n].aad_len -
						ae_cases[n].aad_incr)))
				goto out;
		}

		out_offs = 0;
		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (ae_cases[n].mode == TEE_MODE_ENCRYPT) {
			if (ae_cases[n].ptx != NULL) {
				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_ae_update(c, &session, op,
						ae_cases[n].ptx,
						ae_cases[n].in_incr, out,
						&out_size)))
					goto out;
				out_offs += out_size;
				if (ae_cases[n].algo == TEE_ALG_AES_GCM)
					ADBG_EXPECT_COMPARE_UNSIGNED(c,
					  out_size, ==, ae_cases[n].in_incr);
			}
		} else {
			if (ae_cases[n].ctx != NULL) {
				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_ae_update(c, &session, op,
						ae_cases[n].ctx,
						ae_cases[n].in_incr, out,
						&out_size)))
					goto out;
				out_offs += out_size;
				if (ae_cases[n].algo == TEE_ALG_AES_GCM)
					ADBG_EXPECT_COMPARE_UNSIGNED(c,
					  out_size, ==, ae_cases[n].in_incr);
			}
		}

		out_size = sizeof(out) - out_offs;
		if (ae_cases[n].mode == TEE_MODE_ENCRYPT) {
			uint8_t out_tag[64];
			size_t out_tag_len = MIN(sizeof(out_tag),
						 ae_cases[n].tag_len);

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_ae_encrypt_final(c, &session, op,
					ae_cases[n].ptx + ae_cases[n].in_incr,
					ae_cases[n].ptx_len -
						ae_cases[n].in_incr,
					out + out_offs,
					&out_size, out_tag, &out_tag_len)))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c,
				ae_cases[n].tag, ae_cases[n].tag_len, out_tag,
				out_tag_len);

			out_offs += out_size;

			(void)ADBG_EXPECT_BUFFER(c, ae_cases[n].ctx,
				ae_cases[n].ctx_len, out, out_offs);
		} else {
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_ae_decrypt_final(c, &session, op,
					ae_cases[n].ctx + ae_cases[n].in_incr,
					ae_cases[n].ctx_len -
						ae_cases[n].in_incr,
					out + out_offs,
					&out_size, ae_cases[n].tag,
					ae_cases[n].tag_len)))
				goto out;

			out_offs += out_size;

			(void)ADBG_EXPECT_BUFFER(c, ae_cases[n].ptx,
				ae_cases[n].ptx_len, out, out_offs);
		}

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op)))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
	}
out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4005, xtest_tee_test_4005,
		"Test TEE Internal API Authenticated Encryption operations");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence in implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4212(ADBG_Case_t *c, CK_SLOT_ID slot);

/* AES CMAC test resrouces */
#define CK_AE_KEY_AES(_key_array) \
		{ CKA_ENCRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) }, \
		{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_AES},		\
						sizeof(CK_KEY_TYPE) },	\
		{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},	\
						sizeof(CK_OBJECT_CLASS) }, \
		{ CKA_VALUE, (void *)_key_array, sizeof(_key_array) }

#define CK_AE_AES_CCM_CASE(vect) \
	static CK_ATTRIBUTE cktest_aes_ae_ccm_ ## vect ## _key[] =	\
	{								\
		CK_AE_KEY_AES(ae_data_aes_ccm_ ## vect ## _key)		\
	};								\
									\
	static CK_CCM_PARAMS cktest_aes_ae_ccm_ ## vect ## _params =	\
	{								\
		.ulDataLen = sizeof(ae_data_aes_ccm_ ## vect ## _ptx),	\
		.pNonce = (void *)ae_data_aes_ccm_ ## vect ## _nonce,	\
		.ulNonceLen = sizeof(ae_data_aes_ccm_ ## vect ## _nonce), \
		.pAAD = (void *)ae_data_aes_ccm_ ## vect ## _aad,	\
		.ulAADLen =  sizeof(ae_data_aes_ccm_ ## vect ## _aad),	\
		.ulMACLen = sizeof(ae_data_aes_ccm_ ## vect ## _tag),	\
	};								\
									\
	static CK_MECHANISM cktest_aes_ae_ccm_ ## vect ## _mechanism =	\
	{								\
		.mechanism = CKM_AES_CCM,				\
		.pParameter = (void *)&cktest_aes_ae_ccm_ ## vect ## _params, \
		.ulParameterLen = sizeof(cktest_aes_ae_ccm_ ## vect ## _params), \
	}

#define CK_AE_AES_GCM_CASE(vect) \
	static CK_ATTRIBUTE cktest_aes_ae_gcm_ ## vect ## _key[] =	\
	{								\
		CK_AE_KEY_AES(ae_data_aes_gcm_ ## vect ## _key)		\
	};								\
									\
	static CK_GCM_PARAMS cktest_aes_ae_gcm_ ## vect ## _params =	\
	{								\
		.pIv = (void *)ae_data_aes_gcm_ ## vect ## _nonce,	\
		.ulIvLen = sizeof(ae_data_aes_gcm_ ## vect ## _nonce),	\
		.pAAD =  (void *)ae_data_aes_gcm_ ## vect ## _aad,	\
		.ulAADLen =  sizeof(ae_data_aes_gcm_ ## vect ## _aad),	\
		.ulTagBits = sizeof(ae_data_aes_gcm_ ## vect ## _tag) * 8, \
	};								\
									\
	static CK_MECHANISM cktest_aes_ae_gcm_ ## vect ## _mechanism = { \
		.mechanism = CKM_AES_GCM,				\
		.pParameter = (void *)&cktest_aes_ae_gcm_ ## vect ## _params,	\
		.ulParameterLen = sizeof(cktest_aes_ae_gcm_ ## vect ## _params), \
	}

CK_AE_AES_CCM_CASE(vect1);
CK_AE_AES_CCM_CASE(vect2);
CK_AE_AES_CCM_CASE(vect3);

CK_AE_AES_GCM_CASE(vect1);
CK_AE_AES_GCM_CASE(vect2);
CK_AE_AES_GCM_CASE(vect3);
CK_AE_AES_GCM_CASE(vect4);
CK_AE_AES_GCM_CASE(vect5);
CK_AE_AES_GCM_CASE(vect6);
CK_AE_AES_GCM_CASE(vect7);
CK_AE_AES_GCM_CASE(vect8);
CK_AE_AES_GCM_CASE(vect9);
CK_AE_AES_GCM_CASE(vect10);
CK_AE_AES_GCM_CASE(vect11);
CK_AE_AES_GCM_CASE(vect12);
CK_AE_AES_GCM_CASE(vect13);
CK_AE_AES_GCM_CASE(vect14);
CK_AE_AES_GCM_CASE(vect15);
CK_AE_AES_GCM_CASE(vect16);
CK_AE_AES_GCM_CASE(vect17);
CK_AE_AES_GCM_CASE(vect18);

/* Identify test by the key used to assign the right cryptoki resources */

#define CKTEST_AE_AES_CASE(algo, vect) {	\
	.key =	(void *)ae_data_aes_ ## algo ## _ ## vect ## _key,		\
	.ck_key = (void *)cktest_aes_ae_ ## algo ## _ ## vect ## _key,		\
	.attr_count = ARRAY_SIZE(cktest_aes_ae_ ## algo ## _ ## vect ## _key),	\
	.ck_mechanism = (void *)&cktest_aes_ae_ ## algo ## _ ## vect ## _mechanism }

struct cktest_ae_test_case {
	uint8_t *key;
	CK_ATTRIBUTE_PTR ck_key;
	CK_ULONG attr_count;
	CK_MECHANISM_PTR ck_mechanism;
};

static const struct cktest_ae_test_case cktest_ae_test_case[] = {
	CKTEST_AE_AES_CASE(ccm, vect1),
	CKTEST_AE_AES_CASE(ccm, vect2),
	CKTEST_AE_AES_CASE(ccm, vect3),
	CKTEST_AE_AES_CASE(gcm, vect1),
	CKTEST_AE_AES_CASE(gcm, vect2),
	CKTEST_AE_AES_CASE(gcm, vect3),
	CKTEST_AE_AES_CASE(gcm, vect4),
	CKTEST_AE_AES_CASE(gcm, vect5),
	CKTEST_AE_AES_CASE(gcm, vect6),
	CKTEST_AE_AES_CASE(gcm, vect7),
	CKTEST_AE_AES_CASE(gcm, vect8),
	CKTEST_AE_AES_CASE(gcm, vect9),
	CKTEST_AE_AES_CASE(gcm, vect10),
	CKTEST_AE_AES_CASE(gcm, vect11),
	CKTEST_AE_AES_CASE(gcm, vect12),
	CKTEST_AE_AES_CASE(gcm, vect13),
	CKTEST_AE_AES_CASE(gcm, vect14),
	CKTEST_AE_AES_CASE(gcm, vect15),
	CKTEST_AE_AES_CASE(gcm, vect16),
	CKTEST_AE_AES_CASE(gcm, vect17),
	CKTEST_AE_AES_CASE(gcm, vect18),
};

void run_xtest_tee_test_4212(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	CK_RV rv;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE key_handle;
	uint8_t out[512];
	CK_ULONG out_size;
	size_t out_offs;
	size_t n;
	int close_subcase = 0;
	struct xtest_ae_case const *test;

	rv = C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
			   NULL, 0, &session);
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto out;

	for (n = 0; n < ARRAY_SIZE(ae_cases); n++) {
		CK_ATTRIBUTE_PTR ck_key;
		CK_MECHANISM_PTR mechanism;
		CK_ULONG attr_count = 0;
		size_t i;

		mechanism = NULL;
		ck_key = NULL;

		for (i = 0; i < ARRAY_SIZE(cktest_ae_test_case); i++) {
			if (ae_cases[n].key == cktest_ae_test_case[i].key) {
				ck_key = cktest_ae_test_case[i].ck_key;
				attr_count = cktest_ae_test_case[i].attr_count;
				mechanism = cktest_ae_test_case[i].ck_mechanism;
				break;
			}
		}

		if (!mechanism || !ck_key)
			continue;

		Do_ADBG_BeginSubCase(c, "AE case %d algo 0x%x (%s) %s line %d",
				     (int)n, (unsigned int)ae_cases[n].algo,
				     ckm2str(mechanism->mechanism),
				     (ae_cases[n].mode == TEE_MODE_ENCRYPT) ?
				     "encrypt" : "decrypt",
				     (int)ae_cases[n].line);

		close_subcase = 1;
		test = &ae_cases[n];

		rv = C_CreateObject(session, ck_key, attr_count, &key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		if (test->mode == TEE_MODE_ENCRYPT)
			rv = C_EncryptInit(session, mechanism, key_handle);

		if (test->mode == TEE_MODE_DECRYPT)
			rv = C_DecryptInit(session, mechanism, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		memset(out, 0, sizeof(out));
		out_offs = 0;
		out_size = sizeof(out);

		if (test->mode == TEE_MODE_ENCRYPT && test->ptx) {
			rv = C_EncryptUpdate(session,
					     (void *)test->ptx, test->in_incr,
					     out, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (test->algo == TEE_ALG_AES_GCM)
				ADBG_EXPECT_COMPARE_UNSIGNED(c,
						out_size, ==, test->in_incr);

			out_offs += out_size;
			out_size = sizeof(out) - out_offs;

			rv = C_EncryptUpdate(session,
					     (void *)(test->ptx + test->in_incr),
					     test->ptx_len - test->in_incr,
					     out + out_offs, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			out_offs += out_size;

		} else if (test->mode == TEE_MODE_DECRYPT && test->ctx) {
			rv = C_DecryptUpdate(session,
					     (void *)test->ctx, test->in_incr,
					     out + out_offs, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (!ADBG_EXPECT_COMPARE_UNSIGNED(c,
						(unsigned)out_size, ==, 0))
				goto out;

			out_offs += out_size;
			out_size = sizeof(out) - out_offs;

			rv = C_DecryptUpdate(session,
					     (void *)(test->ctx + test->in_incr),
					     test->ctx_len - test->in_incr,
					     out + out_offs, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (!ADBG_EXPECT_COMPARE_UNSIGNED(c,
						(unsigned)out_size, ==, 0))
				goto out;
		}

		out_size = sizeof(out) - out_offs;

		if (test->mode == TEE_MODE_ENCRYPT) {
			// TODO malloc a bigger buffer: tag stored at the end
			rv = C_EncryptFinal(session, out + out_offs, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (!ADBG_EXPECT_COMPARE_UNSIGNED(c,
						(unsigned)out_size + out_offs,
						==,
						test->ctx_len + test->tag_len))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c, test->ctx, test->ctx_len,
						    out, test->ctx_len);

			(void)ADBG_EXPECT_BUFFER(c, test->tag, test->tag_len,
						    out + test->ctx_len,
						    test->tag_len);
		} else {
			/* Provide the tag as input data in 2 steps */
			if (!ADBG_EXPECT_TRUE(c, !!test->tag))
				goto out;

			out_size = sizeof(out);
			rv = C_DecryptUpdate(session,
					     (void *)(test->tag), 1,
					     out, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (!ADBG_EXPECT_COMPARE_UNSIGNED(c,
						(unsigned)out_size, ==, 0))
				goto out;

			out_size = sizeof(out);
			rv = C_DecryptUpdate(session,
					     (void *)(test->tag + 1),
					     test->tag_len - 1,
					     out, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (!ADBG_EXPECT_COMPARE_UNSIGNED(c,
						(unsigned)out_size, ==, 0))
				goto out;

			out_size = sizeof(out);
			rv = C_DecryptFinal(session, out, &out_size);

			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c, test->ptx, test->ptx_len,
						    out, out_size);
		}

		rv = C_DestroyObject(session, key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
		close_subcase = 0;
	}
out:
	if (close_subcase)
		Do_ADBG_EndSubCase(c, NULL);

	rv = C_CloseSession(session);
	ADBG_EXPECT_CK_OK(c, rv);
}
#endif

struct xtest_ac_case {
	unsigned int level;
	uint32_t algo;
	TEE_OperationMode mode;

	union {
		struct {
			const uint8_t *modulus;
			size_t modulus_len;

			const uint8_t *pub_exp;
			size_t pub_exp_len;

			const uint8_t *priv_exp;
			size_t priv_exp_len;

			const uint8_t *prime1;  /* q */
			size_t prime1_len;
			const uint8_t *prime2;  /* p */
			size_t prime2_len;
			const uint8_t *exp1;    /* dp */
			size_t exp1_len;
			const uint8_t *exp2;    /* dq */
			size_t exp2_len;
			const uint8_t *coeff;   /* iq */
			size_t coeff_len;

			int salt_len;
		} rsa;
		struct {
			const uint8_t *prime;
			size_t prime_len;
			const uint8_t *sub_prime;
			size_t sub_prime_len;
			const uint8_t *base;
			size_t base_len;
			const uint8_t *pub_val;
			size_t pub_val_len;
			const uint8_t *priv_val;
			size_t priv_val_len;
		} dsa;
		struct {
			const uint8_t *private;
			size_t private_len;
			const uint8_t *public_x;
			size_t public_x_len;
			const uint8_t *public_y;
			size_t public_y_len;
		} ecdsa;
	} params;

	const uint8_t *ptx;
	size_t ptx_len;
	const uint8_t *ctx;
	size_t ctx_len;
	size_t line;
};

#define WITHOUT_SALT(x) -1
#define WITH_SALT(x)    x

#define XTEST_AC_CASE(level, algo, mode, vect, union_params) \
	{ level, (algo), (mode), .params = union_params, \
	  ARRAY(vect ## _ptx), \
	  ARRAY(vect ## _out), \
	  __LINE__ }

#define XTEST_AC_RSA_UNION(vect, opt_crt_array, opt_salt) \
	{ .rsa = { \
		  ARRAY(vect ## _modulus), \
		  ARRAY(vect ## _pub_exp), \
		  ARRAY(vect ## _priv_exp), \
		  opt_crt_array(vect ## _prime1), \
		  opt_crt_array(vect ## _prime2), \
		  opt_crt_array(vect ## _exp1), \
		  opt_crt_array(vect ## _exp2), \
		  opt_crt_array(vect ## _coeff), \
		  opt_salt(vect ## _salt_len) \
	  } }

#define XTEST_AC_RSA_CASE(level, algo, mode, vect, opt_crt_array, opt_salt) \
	XTEST_AC_CASE(level, algo, mode, vect, \
		      XTEST_AC_RSA_UNION(vect, opt_crt_array, opt_salt))

#define XTEST_AC_DSA_UNION(vect) \
	{ .dsa = { \
		  ARRAY(vect ## _prime), \
		  ARRAY(vect ## _sub_prime), \
		  ARRAY(vect ## _base), \
		  ARRAY(vect ## _pub_val), \
		  ARRAY(vect ## _priv_val), \
	  } }

#define XTEST_AC_DSA_CASE(level, algo, mode, vect) \
	XTEST_AC_CASE(level, algo, mode, vect, XTEST_AC_DSA_UNION(vect))

#define XTEST_AC_ECDSA_UNION(vect) \
	{ .ecdsa = { \
		  ARRAY(vect ## _private), \
		  ARRAY(vect ## _public_x), \
		  ARRAY(vect ## _public_y), \
	  } }

#define XTEST_AC_ECDSA_CASE(level, algo, mode, vect) \
	XTEST_AC_CASE(level, algo, mode, vect, XTEST_AC_ECDSA_UNION(vect))

static const struct xtest_ac_case xtest_ac_cases[] = {
	/* RSA test without crt parameters */
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect18, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect18, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect19, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect19, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect3, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect3, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect4, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect4, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect5, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect5, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect6, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect6, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect7, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect7, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect8, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect8, NULL_ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA224, TEE_MODE_SIGN,
			  ac_rsassa_vect16, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA224, TEE_MODE_VERIFY,
			  ac_rsassa_vect16, NULL_ARRAY, WITHOUT_SALT),

#ifdef CFG_CRYPTO_RSASSA_NA1
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5, TEE_MODE_SIGN,
			  ac_rsassa_vect20, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5, TEE_MODE_VERIFY,
			  ac_rsassa_vect20, NULL_ARRAY, WITHOUT_SALT),
#endif

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA256, TEE_MODE_SIGN,
			  ac_rsassa_vect9, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA256, TEE_MODE_VERIFY,
			  ac_rsassa_vect9, NULL_ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA384, TEE_MODE_SIGN,
			  ac_rsassa_vect10, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA384, TEE_MODE_VERIFY,
			  ac_rsassa_vect10, NULL_ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA512, TEE_MODE_SIGN,
			  ac_rsassa_vect11, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA512, TEE_MODE_VERIFY,
			  ac_rsassa_vect11, NULL_ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect12, NULL_ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect12, NULL_ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect17, NULL_ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect17, NULL_ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect13, NULL_ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect13, NULL_ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect14, NULL_ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect14, NULL_ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect15, NULL_ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect15, NULL_ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect15, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect15, NULL_ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect1, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect2, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect10, NULL_ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect10, NULL_ARRAY, WITHOUT_SALT),

	/* RSA test with crt parameters */
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT,
			  ac_rsassa_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT,
			  ac_rsassa_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect3, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect3, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect4, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect4, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect5, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect5, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect6, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect6, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect7, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect7, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect8, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			  ac_rsassa_vect8, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA256, TEE_MODE_SIGN,
			  ac_rsassa_vect9, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_V1_5_SHA256, TEE_MODE_VERIFY,
			  ac_rsassa_vect9, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA384, TEE_MODE_SIGN,
			  ac_rsassa_vect10, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA384, TEE_MODE_VERIFY,
			  ac_rsassa_vect10, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA512, TEE_MODE_SIGN,
			  ac_rsassa_vect11, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_V1_5_SHA512, TEE_MODE_VERIFY,
			  ac_rsassa_vect11, ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect12, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1,TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1, TEE_MODE_SIGN,
			  ac_rsassa_vect12, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect17, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect17, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect13, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect13, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect14, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect14, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512,
			  TEE_MODE_VERIFY,
			  ac_rsassa_vect15, ARRAY, WITH_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512,
			  TEE_MODE_SIGN,
			  ac_rsassa_vect15, ARRAY, WITH_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_DECRYPT,
			  ac_rsaes_pkcs1_v1_5_vect15, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_V1_5, TEE_MODE_ENCRYPT,
			  ac_rsaes_pkcs1_v1_5_vect15, ARRAY, WITHOUT_SALT),

	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(0, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect1, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect2, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_DECRYPT,
			  ac_rsaes_oaep_vect10, ARRAY, WITHOUT_SALT),
	XTEST_AC_RSA_CASE(1, TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,
			  TEE_MODE_ENCRYPT,
			  ac_rsaes_oaep_vect10, ARRAY, WITHOUT_SALT),

	/* DSA tests */
	/* [mod = L=1024, N=160, SHA-1] */
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect1),
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect1),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect2),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect2),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect3),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect3),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect4),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect4),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect5),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect5),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect6),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect6),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect7),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect7),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect8),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect8),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect9),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect9),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect10),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect10),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect11),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect11),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect12),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect12),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect13),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect13),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect14),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect14),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_VERIFY, ac_dsa_vect15),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA1, TEE_MODE_SIGN, ac_dsa_vect15),
	/* [mod = L=1024, N=160, SHA-224] - GP NOT SUPPORTED */
	/* [mod = L=1024, N=160, SHA-256] - GP NOT SUPPORTED */
	/* [mod = L=1024, N=160, SHA-384] - GP NOT SUPPORTED */
	/* [mod = L=1024, N=160, SHA-512] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=224, SHA-1] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=224, SHA-224] */
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect91),
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect91),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect92),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect92),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect93),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect93),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect94),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect94),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect95),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect95),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect96),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect96),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect97),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect97),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect98),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect98),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect99),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect99),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect100),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect100),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect101),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect101),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect102),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect102),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect103),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect103),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect104),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect104),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect105),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect105),
	/* [mod = L=2048, N=224, SHA-256] */
	XTEST_AC_DSA_CASE(0, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect106),
	XTEST_AC_DSA_CASE(0, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect106),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect107),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect107),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect108),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect108),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect109),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect109),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect110),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect110),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect111),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect111),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect112),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect112),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect113),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect113),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect114),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect114),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect115),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect115),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect116),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect116),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect117),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect117),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect118),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect118),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect119),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect119),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect120),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect120),
	/* [mod = L=2048, N=224, SHA-384] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=224, SHA-512] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=256, SHA-1] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=256, SHA-224] */
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect166),
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect166),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect167),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect167),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect168),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect168),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect169),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect169),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect170),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect170),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect171),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect171),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect172),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect172),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect173),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect173),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect174),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect174),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect175),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect175),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect176),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect176),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect177),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect177),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect178),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect178),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect179),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect179),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_VERIFY, ac_dsa_vect180),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA224, TEE_MODE_SIGN, ac_dsa_vect180),
	/* [mod = L=2048, N=256, SHA-256] */
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect181),
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect181),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect182),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect182),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect183),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect183),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect184),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect184),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect185),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect185),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect186),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect186),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect187),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect187),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect188),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect188),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect189),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect189),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect190),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect190),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect191),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect191),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect192),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect192),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect193),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect193),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect194),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect194),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect195),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect195),
	/* [mod = L=2048, N=256, SHA-384] - GP NOT SUPPORTED */
	/* [mod = L=2048, N=256, SHA-512] - GP NOT SUPPORTED */
	/* [mod = L=3072, N=256, SHA-1] - GP NOT SUPPORTED */
	/* [mod = L=3072, N=256, SHA-224] - GP NOT SUPPORTED */
	/* [mod = L=3072, N=256, SHA-256] */
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect256),
	XTEST_AC_DSA_CASE(1, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect256),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect257),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect257),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect258),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect258),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect259),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect259),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect260),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect260),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect261),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect261),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect262),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect262),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect263),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect263),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect264),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect264),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect265),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect265),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect266),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect266),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect267),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect267),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect268),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect268),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect269),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect269),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_VERIFY, ac_dsa_vect270),
	XTEST_AC_DSA_CASE(15, TEE_ALG_DSA_SHA256, TEE_MODE_SIGN, ac_dsa_vect270),
	/* [mod = L=3072, N=256, SHA-384] - GP NOT SUPPORTED */
	/* [mod = L=3072, N=256, SHA-512] - GP NOT SUPPORTED */

	/* ECDSA tests */
	/* [P-192] */
	XTEST_AC_ECDSA_CASE(0, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_1),
	XTEST_AC_ECDSA_CASE(0, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_1),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_2),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_2),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_3),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_3),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_4),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_4),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_5),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_5),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_6),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_6),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_7),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_7),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_8),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_8),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_9),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_9),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_10),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_10),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_11),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_11),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_12),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_12),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_13),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_13),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_14),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_14),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_15),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P192, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_15),
	/* [P-224] */
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_16),
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_16),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_17),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_17),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_18),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_18),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_19),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_19),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_20),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_20),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_21),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_21),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_22),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_22),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_23),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_23),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_24),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_24),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_25),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_25),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_26),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_26),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_27),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_27),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_28),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_28),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_29),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_29),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_30),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P224, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_30),
	/* [P-256] */
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_31),
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_31),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_32),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_32),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_33),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_33),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_34),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_34),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_35),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_35),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_36),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_36),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_37),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_37),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_38),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_38),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_39),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_39),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_40),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_40),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_41),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_41),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_42),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_42),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_43),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_43),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_44),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_44),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_45),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P256, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_45),
	/* [P-384] */
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_46),
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_46),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_47),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_47),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_48),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_48),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_49),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_49),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_50),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_50),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_51),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_51),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_52),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_52),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_53),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_53),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_54),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_54),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_55),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_55),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_56),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_56),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_57),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_57),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_58),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_58),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_59),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_59),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_60),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P384, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_60),
	/* [P-521] */
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_61),
	XTEST_AC_ECDSA_CASE(1, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_61),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_62),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_62),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_63),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_63),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_64),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_64),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_65),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_65),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_66),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_66),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_67),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_67),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_68),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_68),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_69),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_69),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_70),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_70),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_71),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_71),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_72),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_72),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_73),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_73),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_74),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_74),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_VERIFY,
			    nist_186_2_ecdsa_testvector_75),
	XTEST_AC_ECDSA_CASE(15, TEE_ALG_ECDSA_P521, TEE_MODE_SIGN,
			    nist_186_2_ecdsa_testvector_75),
	/* [K-163] - GP NOT SUPPORTED */
	/* [K-233] - GP NOT SUPPORTED */
	/* [K-283] - GP NOT SUPPORTED */
	/* [K-409] - GP NOT SUPPORTED */
	/* [K-571] - GP NOT SUPPORTED */
	/* [B-163] - GP NOT SUPPORTED */
	/* [B-233] - GP NOT SUPPORTED */
	/* [B-283] - GP NOT SUPPORTED */
	/* [B-409] - GP NOT SUPPORTED */
	/* [B-571] - GP NOT SUPPORTED */
};

static bool create_key(ADBG_Case_t *c, TEEC_Session *s,
		       uint32_t max_key_size, uint32_t key_type,
		       TEE_Attribute *attrs, size_t num_attrs,
		       TEE_ObjectHandle *handle)
{
	size_t n;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_transient_object(c, s, key_type,
			max_key_size, handle)))
		return false;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_populate_transient_object(c, s, *handle, attrs,
			num_attrs)))
		return false;

	for (n = 0; n < num_attrs; n++) {
		uint8_t out[512];
		size_t out_size;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));

		if (attrs[n].attributeID == TEE_ATTR_ECC_CURVE)
			continue;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_get_object_buffer_attribute(c, s, *handle,
				attrs[n].attributeID, out, &out_size)))
			return false;

		if (out_size < attrs[n].content.ref.length) {
			memmove(out + (attrs[n].content.ref.length - out_size),
				out,
				attrs[n].content.ref.length);
			memset(out, 0, attrs[n].content.ref.length - out_size);
			out_size = attrs[n].content.ref.length;
		}

		if (!ADBG_EXPECT_BUFFER(c, attrs[n].content.ref.buffer,
			attrs[n].content.ref.length, out, out_size))
			return false;
	}

	return true;
}

static void xtest_tee_test_4006(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	TEE_OperationHandle op = TEE_HANDLE_NULL;
	TEE_ObjectHandle priv_key_handle = TEE_HANDLE_NULL;
	TEE_ObjectHandle pub_key_handle = TEE_HANDLE_NULL;
	TEE_Attribute key_attrs[8];
	TEE_Attribute algo_params[1];
	size_t num_algo_params;
	uint8_t out[512];
	size_t out_size;
	uint8_t out_enc[512];
	size_t out_enc_size;
	uint8_t ptx_hash[TEE_MAX_HASH_SIZE];
	size_t ptx_hash_size = 0;
	size_t max_key_size;
	size_t num_key_attrs;
	uint32_t ret_orig;
	size_t n;
	uint32_t curve;
	uint32_t hash_algo;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
			&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(xtest_ac_cases); n++) {
		const struct xtest_ac_case *tv = xtest_ac_cases + n;

		if (tv->level > level)
			continue;

		Do_ADBG_BeginSubCase(c, "Asym Crypto case %d algo 0x%x line %d",
				     (int)n, (unsigned int)tv->algo,
				     (int)tv->line);

		/*
		 * When signing or verifying we're working with the hash of
		 * the payload.
		 */
		if (tv->mode == TEE_MODE_VERIFY || tv->mode == TEE_MODE_SIGN) {
			if (TEE_ALG_GET_MAIN_ALG(tv->algo) == TEE_MAIN_ALGO_ECDSA)
				hash_algo = TEE_ALG_SHA1;
#if defined(CFG_CRYPTO_RSASSA_NA1)
			else if (tv->algo == TEE_ALG_RSASSA_PKCS1_V1_5)
				hash_algo = TEE_ALG_SHA256;
#endif
			else
				hash_algo = TEE_ALG_HASH_ALGO(
					TEE_ALG_GET_DIGEST_HASH(tv->algo));

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, hash_algo, TEE_MODE_DIGEST, 0)))
				goto out;

			ptx_hash_size = sizeof(ptx_hash);
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_digest_do_final(c, & session, op,
					tv->ptx, tv->ptx_len, ptx_hash,
					&ptx_hash_size)))
				goto out;

			/*
			 * When we use DSA algorithms, the size of the hash we
			 * consider equals the min between the size of the
			 * "subprime" in the key and the size of the hash
			 */
			if (TEE_ALG_GET_MAIN_ALG(tv->algo) ==
			    TEE_MAIN_ALGO_DSA) {
				if (tv->params.dsa.sub_prime_len <=
				    ptx_hash_size)
					ptx_hash_size =
						tv->params.dsa.sub_prime_len;
			}

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_operation(c, &session, op)))
				goto out;
		}

		num_algo_params = 0;
		num_key_attrs = 0;
		switch (TEE_ALG_GET_MAIN_ALG(tv->algo)) {
		case TEE_MAIN_ALGO_RSA:
			if (tv->params.rsa.salt_len > 0) {
				algo_params[0].attributeID =
					TEE_ATTR_RSA_PSS_SALT_LENGTH;
				algo_params[0].content.value.a =
					tv->params.rsa.salt_len;
				algo_params[0].content.value.b = 0;
				num_algo_params = 1;
			}

			max_key_size = tv->params.rsa.modulus_len * 8;

			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_RSA_MODULUS,
				       tv->params.rsa.modulus,
				       tv->params.rsa.modulus_len);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_RSA_PUBLIC_EXPONENT,
				       tv->params.rsa.pub_exp,
				       tv->params.rsa.pub_exp_len);

			if (!ADBG_EXPECT_TRUE(c,
				create_key(c, &session,
					   max_key_size,
					   TEE_TYPE_RSA_PUBLIC_KEY,
					   key_attrs,
					   num_key_attrs,
					   &pub_key_handle)))
				goto out;

			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_RSA_PRIVATE_EXPONENT,
				       tv->params.rsa.priv_exp,
				       tv->params.rsa.priv_exp_len);

			if (tv->params.rsa.prime1_len != 0) {
				xtest_add_attr(&num_key_attrs, key_attrs,
					       TEE_ATTR_RSA_PRIME1,
					       tv->params.rsa.prime1,
					       tv->params.rsa.prime1_len);
			}

			if (tv->params.rsa.prime2_len != 0) {
				xtest_add_attr(&num_key_attrs, key_attrs,
			       TEE_ATTR_RSA_PRIME2,
			       tv->params.rsa.prime2,
			       tv->params.rsa.prime2_len);
			}

			if (tv->params.rsa.exp1_len != 0) {
				xtest_add_attr(&num_key_attrs, key_attrs,
			       TEE_ATTR_RSA_EXPONENT1,
			       tv->params.rsa.exp1,
			       tv->params.rsa.exp1_len);
			}

			if (tv->params.rsa.exp2_len != 0) {
				xtest_add_attr(&num_key_attrs, key_attrs,
			       TEE_ATTR_RSA_EXPONENT2,
			       tv->params.rsa.exp2,
			       tv->params.rsa.exp2_len);
			}

			if (tv->params.rsa.coeff_len != 0) {
				xtest_add_attr(&num_key_attrs, key_attrs,
			       TEE_ATTR_RSA_COEFFICIENT,
			       tv->params.rsa.coeff,
			       tv->params.rsa.coeff_len);
			}

			if (!ADBG_EXPECT_TRUE(c,
			      create_key(c, &session,
				 max_key_size,
				 TEE_TYPE_RSA_KEYPAIR,
				 key_attrs,
				 num_key_attrs,
				 &priv_key_handle)))
				goto out;
			break;

		case TEE_MAIN_ALGO_DSA:
			max_key_size = tv->params.dsa.prime_len * 8;

			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_DSA_PRIME,
				       tv->params.dsa.prime,
				       tv->params.dsa.prime_len);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_DSA_SUBPRIME,
				       tv->params.dsa.sub_prime,
				       tv->params.dsa.sub_prime_len);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_DSA_BASE,
				       tv->params.dsa.base,
				       tv->params.dsa.base_len);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_DSA_PUBLIC_VALUE,
				       tv->params.dsa.pub_val,
				       tv->params.dsa.pub_val_len);

			if (!ADBG_EXPECT_TRUE(c,
				create_key(c, &session, max_key_size,
					   TEE_TYPE_DSA_PUBLIC_KEY, key_attrs,
					   num_key_attrs, &pub_key_handle)))
				goto out;

			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_DSA_PRIVATE_VALUE,
				       tv->params.dsa.priv_val,
				       tv->params.dsa.priv_val_len);

			if (!ADBG_EXPECT_TRUE(c,
				create_key(c, &session, max_key_size,
					   TEE_TYPE_DSA_KEYPAIR, key_attrs,
					   num_key_attrs, &priv_key_handle)))
				goto out;
			break;

		case TEE_MAIN_ALGO_ECDSA:
			switch (tv->algo) {
			case TEE_ALG_ECDSA_P192:
				curve = TEE_ECC_CURVE_NIST_P192;
				break;
			case TEE_ALG_ECDSA_P224:
				curve = TEE_ECC_CURVE_NIST_P224;
				break;
			case TEE_ALG_ECDSA_P256:
				curve = TEE_ECC_CURVE_NIST_P256;
				break;
			case TEE_ALG_ECDSA_P384:
				curve = TEE_ECC_CURVE_NIST_P384;
				break;
			case TEE_ALG_ECDSA_P521:
				curve = TEE_ECC_CURVE_NIST_P521;
				break;
			default:
				curve = 0xFF;
				break;
			}

			if (tv->algo == TEE_ALG_ECDSA_P521)
				max_key_size = 521;
			else
				max_key_size = tv->params.ecdsa.private_len * 8;

			xtest_add_attr_value(&num_key_attrs, key_attrs,
					     TEE_ATTR_ECC_CURVE, curve, 0);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_ECC_PUBLIC_VALUE_X,
				       tv->params.ecdsa.public_x,
				       tv->params.ecdsa.public_x_len);
			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_ECC_PUBLIC_VALUE_Y,
				       tv->params.ecdsa.public_y,
				       tv->params.ecdsa.public_y_len);

			if (!ADBG_EXPECT_TRUE(c,
				create_key(c, &session, max_key_size,
					   TEE_TYPE_ECDSA_PUBLIC_KEY, key_attrs,
					   num_key_attrs, &pub_key_handle)))
				goto out;

			xtest_add_attr(&num_key_attrs, key_attrs,
				       TEE_ATTR_ECC_PRIVATE_VALUE,
				       tv->params.ecdsa.private,
				       tv->params.ecdsa.private_len);

			if (!ADBG_EXPECT_TRUE(c,
				create_key(c, &session, max_key_size,
					   TEE_TYPE_ECDSA_KEYPAIR, key_attrs,
					   num_key_attrs, &priv_key_handle)))
				goto out;
			break;

		default:
			ADBG_EXPECT_TRUE(c, false);
			goto out;
		}

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		switch (tv->mode) {
		case TEE_MODE_ENCRYPT:
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, tv->algo, TEE_MODE_ENCRYPT,
					max_key_size)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					pub_key_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, &session,
					pub_key_handle)))
				goto out;
			pub_key_handle = TEE_HANDLE_NULL;

			out_enc_size = sizeof(out_enc);
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_asymmetric_encrypt(c, &session, op,
					NULL, 0, tv->ptx, tv->ptx_len, out_enc,
					&out_enc_size)))
				goto out;

			/*
			 * A PS which is random is added when formatting the
			 * message internally of the algorithm so we can't
			 * verify against precomputed values, instead we use the
			 * decrypt operation to see that output is correct.
			 */

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_operation(c, &session, op)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, tv->algo, TEE_MODE_DECRYPT,
					max_key_size)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					priv_key_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, &session,
					priv_key_handle)))
				goto out;

			priv_key_handle = TEE_HANDLE_NULL;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_asymmetric_decrypt(c, &session, op,
					NULL, 0, out_enc, out_enc_size, out,
					&out_size)))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c, tv->ptx, tv->ptx_len, out,
						 out_size);
			break;

		case TEE_MODE_DECRYPT:
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, tv->algo, TEE_MODE_DECRYPT,
					max_key_size)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					priv_key_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, &session,
					priv_key_handle)))
				goto out;

			priv_key_handle = TEE_HANDLE_NULL;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_asymmetric_decrypt(c, &session, op,
					NULL, 0, tv->ctx, tv->ctx_len, out,
					&out_size)))
				goto out;

			(void)ADBG_EXPECT_BUFFER(c, tv->ptx, tv->ptx_len, out,
						 out_size);
			break;

		case TEE_MODE_VERIFY:
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, tv->algo, TEE_MODE_VERIFY,
					max_key_size)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					pub_key_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, &session,
					pub_key_handle)))
				goto out;

			pub_key_handle = TEE_HANDLE_NULL;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_asymmetric_verify(c, &session, op,
					algo_params, num_algo_params, ptx_hash,
					ptx_hash_size, tv->ctx, tv->ctx_len)))
				goto out;
			break;

		case TEE_MODE_SIGN:
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c, &session,
					&op, tv->algo, TEE_MODE_SIGN,
					max_key_size)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					priv_key_handle)))
				goto out;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, &session,
					priv_key_handle)))
				goto out;

			priv_key_handle = TEE_HANDLE_NULL;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_asymmetric_sign(c, &session, op,
					algo_params, num_algo_params, ptx_hash,
					ptx_hash_size, out, &out_size)))
				goto out;

			if (TEE_ALG_GET_CHAIN_MODE(tv->algo) ==
			    TEE_CHAIN_MODE_PKCS1_PSS_MGF1 ||
			    tv->algo == TEE_ALG_DSA_SHA1 ||
			    tv->algo == TEE_ALG_DSA_SHA224 ||
			    tv->algo == TEE_ALG_DSA_SHA256 ||
			    TEE_ALG_GET_MAIN_ALG(tv->algo) ==
					    TEE_MAIN_ALGO_ECDSA) {
				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_free_operation(c, &session,
								    op)))
					goto out;
				/*
				 * The salt or K is random so we can't verify
				 * signing against precomputed values, instead
				 * we use the verify operation to see that
				 * output is correct.
				 */
				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_allocate_operation(c,
						&session, &op, tv->algo,
						TEE_MODE_VERIFY, max_key_size)))
					goto out;

				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_set_operation_key(c,
						&session, op, pub_key_handle)))
					goto out;

				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_free_transient_object(c,
						&session, pub_key_handle)))
					goto out;

				pub_key_handle = TEE_HANDLE_NULL;

				if (!ADBG_EXPECT_TEEC_SUCCESS(c,
					ta_crypt_cmd_asymmetric_verify(c,
						&session, op, algo_params,
						num_algo_params, ptx_hash,
						ptx_hash_size, out, out_size)))
					goto out;
			} else {
				(void)ADBG_EXPECT_BUFFER(c, tv->ctx,
							 tv->ctx_len, out,
							 out_size);
			}
			break;

		default:
			break;
		}

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				pub_key_handle)))
			goto out;
		pub_key_handle = TEE_HANDLE_NULL;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
				priv_key_handle)))
			goto out;

		priv_key_handle = TEE_HANDLE_NULL;

		Do_ADBG_EndSubCase(c, NULL);
	}
out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4006, xtest_tee_test_4006,
		"Test TEE Internal API Asymmetric Cipher operations");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence is implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4217(ADBG_Case_t *c, CK_SLOT_ID slot);

static CK_UTF8CHAR label_rsa_pub[] = "Generic RSA public key for testing";
static CK_ATTRIBUTE rsa_key_pub_attr[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PUBLIC_KEY},
		sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_RSA}, sizeof(CK_KEY_TYPE) },
	{ CKA_LABEL, label_rsa_pub, sizeof(label_rsa_pub) - 1 },
	{ CKA_VERIFY, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_ENCRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_MODULUS, (void *)NULL, 0 },
	{ CKA_PUBLIC_EXPONENT, (void *)NULL, 0 },
};

static CK_UTF8CHAR label_rsa_priv[] = "Generic RSA private key for testing";
static CK_ATTRIBUTE rsa_key_priv_attr[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PRIVATE_KEY},
		sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_RSA}, sizeof(CK_KEY_TYPE) },
	{ CKA_LABEL, label_rsa_priv, sizeof(label_rsa_priv) - 1 },
	{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_MODULUS, (void *)NULL, 0 },
	{ CKA_PUBLIC_EXPONENT, (void *)NULL, 0 },
	{ CKA_PRIVATE_EXPONENT, (void *)NULL, 0 },
	{ CKA_PRIME_1, (void *)NULL, 0 },
	{ CKA_PRIME_2, (void *)NULL, 0 },
	{ CKA_EXPONENT_1, (void *)NULL, 0 },
	{ CKA_EXPONENT_2, (void *)NULL, 0 },
	{ CKA_COEFFICIENT, (void *)NULL, 0 },
};

static CK_ATTRIBUTE rsa_key_priv_attr2[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PRIVATE_KEY},
		sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_RSA}, sizeof(CK_KEY_TYPE) },
	{ CKA_LABEL, label_rsa_priv, sizeof(label_rsa_priv) - 1 },
	{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_MODULUS, (void *)NULL, 0 },
	{ CKA_PUBLIC_EXPONENT, (void *)NULL, 0 },
	{ CKA_PRIVATE_EXPONENT, (void *)NULL, 0 },
};

static CK_UTF8CHAR label_ec_pub[] = "Generic EC public key for testing";
static CK_ATTRIBUTE __unused cktest_ec_key_pub_attr[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PUBLIC_KEY},
		sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_EC}, sizeof(CK_KEY_TYPE) },
	{ CKA_VERIFY, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_LABEL, label_ec_pub, sizeof(label_ec_pub) - 1 },
	{ CKA_EC_PARAMS, (void *)NULL, 0 },		// to fill at runtime
	{ CKA_EC_POINT, (void *)NULL, 0 },		// to fill at runtime
	{ CKA_VENDOR_EC_POINT_Y, (void *)NULL, 0 },	// to fill at runtime
	{ CKA_VENDOR_EC_POINT_X, (void *)NULL, 0 },	// to fill at runtime
};

CK_UTF8CHAR label_ec_priv[] = "Generic EC private key for testing";
CK_ATTRIBUTE cktest_ec_key_priv_attr[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PRIVATE_KEY},
		sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_EC}, sizeof(CK_KEY_TYPE) },
	{ CKA_SENSITIVE, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_DERIVE, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_LABEL, label_ec_priv, sizeof(label_ec_priv) - 1 },
	{ CKA_SUBJECT, (void *)NULL, 0 },
	{ CKA_EC_PARAMS, (void *)NULL, 0 },
	{ CKA_VALUE, (void *)NULL, 0 },
	{ CKA_VENDOR_EC_POINT_Y, (void *)NULL, 0 },	// to fill at runtime
	{ CKA_VENDOR_EC_POINT_X, (void *)NULL, 0 },	// to fill at runtime
};

#define CKTEST_RSA_PSS_PARAMS(_label, _algo, _mgf)	\
		CK_RSA_PKCS_PSS_PARAMS _label = {	\
			.hashAlg = _algo,		\
			.mgf = _mgf,			\
		}

#define CKTEST_RSA_OAEP_PARAMS(_label, _algo, _mgf)	\
		CK_RSA_PKCS_OAEP_PARAMS _label = {	\
			.hashAlg = _algo,		\
			.mgf = _mgf,			\
			.source = CKZ_DATA_SPECIFIED,	\
		};

/*
 * This test assumes the regression 4006 uses the same hash algorithm for
 * the mask generation function and the lessage. CK parameters not set below
 * at set at runtime from regression 4006 test materials.
 */
static CKTEST_RSA_PSS_PARAMS(pss_sha1_params, CKM_SHA_1, CKG_MGF1_SHA1);
static CKTEST_RSA_PSS_PARAMS(pss_sha224_params, CKM_SHA224, CKG_MGF1_SHA224);
static CKTEST_RSA_PSS_PARAMS(pss_sha256_params, CKM_SHA256, CKG_MGF1_SHA256);
static CKTEST_RSA_PSS_PARAMS(pss_sha384_params, CKM_SHA384, CKG_MGF1_SHA384);
static CKTEST_RSA_PSS_PARAMS(pss_sha512_params, CKM_SHA512, CKG_MGF1_SHA512);

static CKTEST_RSA_OAEP_PARAMS(oaep_sha1_params, CKM_SHA_1, CKG_MGF1_SHA1);
static CKTEST_RSA_OAEP_PARAMS(oaep_sha224_params, CKM_SHA224, CKG_MGF1_SHA224);
static CKTEST_RSA_OAEP_PARAMS(oaep_sha256_params, CKM_SHA256, CKG_MGF1_SHA256);
static CKTEST_RSA_OAEP_PARAMS(oaep_sha384_params, CKM_SHA384, CKG_MGF1_SHA384);
static CKTEST_RSA_OAEP_PARAMS(oaep_sha512_params, CKM_SHA512, CKG_MGF1_SHA512);

static CK_ECDH1_DERIVE_PARAMS cktest_ecdh_params = {
	.kdf = CKD_NULL,
	.ulSharedDataLen = 0,
	.pSharedData = NULL,
	.ulPublicDataLen = 0,	/* Set at runtime */
	.pPublicData = NULL,	/* Set at runtime */
};

/* Convert a TEE algorithm into a PKCS#11 CK mechanism identifier */
struct mechanism_converter {
	CK_MECHANISM_TYPE ckMechanismType;
	CK_VOID_PTR ckParameter;
	CK_ULONG ckParameterLen;	/* in bytes */
	uint32_t tee_algo;
};

#define MECHA_CONV_ITEM(_mecha, _params, _tee_algo)	\
	{							\
		.ckMechanismType = (CK_MECHANISM_TYPE)(_mecha),	\
		.ckParameter = &_params,			\
		.ckParameterLen = sizeof(_params),		\
		.tee_algo = (uint32_t)(_tee_algo),		\
	}

#define MECHA_CONV_NOPARAM(_mecha, _tee_algo)	\
	{							\
		.ckMechanismType = (CK_MECHANISM_TYPE)(_mecha),	\
		.ckParameter = NULL,				\
		.ckParameterLen = 0,				\
		.tee_algo = (uint32_t)(_tee_algo),		\
	}

static struct mechanism_converter mechanism_converter[] = {
	MECHA_CONV_NOPARAM(CKM_SHA1_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5_SHA1),
	MECHA_CONV_NOPARAM(CKM_SHA224_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5_SHA224),
	MECHA_CONV_NOPARAM(CKM_SHA256_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5_SHA256),
	MECHA_CONV_NOPARAM(CKM_SHA384_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5_SHA384),
	MECHA_CONV_NOPARAM(CKM_SHA512_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5_SHA512),

	MECHA_CONV_ITEM(CKM_SHA1_RSA_PKCS_PSS, pss_sha1_params,
			TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1),
	MECHA_CONV_ITEM(CKM_SHA224_RSA_PKCS_PSS, pss_sha224_params,
			TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224),
	MECHA_CONV_ITEM(CKM_SHA256_RSA_PKCS_PSS, pss_sha256_params,
			TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256),
	MECHA_CONV_ITEM(CKM_SHA384_RSA_PKCS_PSS, pss_sha384_params,
			TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384),
	MECHA_CONV_ITEM(CKM_SHA512_RSA_PKCS_PSS, pss_sha512_params,
			TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512),

	MECHA_CONV_NOPARAM(CKM_RSA_PKCS,
			   TEE_ALG_RSAES_PKCS1_V1_5),
	MECHA_CONV_NOPARAM(CKM_RSA_PKCS,
			   TEE_ALG_RSASSA_PKCS1_V1_5),

	MECHA_CONV_ITEM(CKM_RSA_PKCS_OAEP, oaep_sha1_params,
			TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1),
	MECHA_CONV_ITEM(CKM_RSA_PKCS_OAEP, oaep_sha224_params,
			TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224),
	MECHA_CONV_ITEM(CKM_RSA_PKCS_OAEP, oaep_sha256_params,
			TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256),
	MECHA_CONV_ITEM(CKM_RSA_PKCS_OAEP, oaep_sha384_params,
			TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384),
	MECHA_CONV_ITEM(CKM_RSA_PKCS_OAEP, oaep_sha512_params,
			TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512),

	MECHA_CONV_NOPARAM(CKM_ECDSA, TEE_ALG_ECDSA_P192),
	MECHA_CONV_NOPARAM(CKM_ECDSA, TEE_ALG_ECDSA_P224),
	MECHA_CONV_NOPARAM(CKM_ECDSA, TEE_ALG_ECDSA_P256),
	MECHA_CONV_NOPARAM(CKM_ECDSA, TEE_ALG_ECDSA_P384),
	MECHA_CONV_NOPARAM(CKM_ECDSA, TEE_ALG_ECDSA_P521),

	MECHA_CONV_ITEM(CKM_ECDH1_DERIVE, cktest_ecdh_params,
			TEE_ALG_ECDH_P192),
	MECHA_CONV_ITEM(CKM_ECDH1_DERIVE, cktest_ecdh_params,
			TEE_ALG_ECDH_P224),
	MECHA_CONV_ITEM(CKM_ECDH1_DERIVE, cktest_ecdh_params,
			TEE_ALG_ECDH_P256),
	MECHA_CONV_ITEM(CKM_ECDH1_DERIVE, cktest_ecdh_params,
			TEE_ALG_ECDH_P384),
	MECHA_CONV_ITEM(CKM_ECDH1_DERIVE, cktest_ecdh_params,
			TEE_ALG_ECDH_P521),
};

static int tee_alg2ckmt(uint32_t tee_alg, CK_MECHANISM_PTR mecha)
{
	struct mechanism_converter *conv = mechanism_converter;
	size_t n;

	for (n = 0; n < ARRAY_SIZE(mechanism_converter); n++, conv++) {
		if (conv->tee_algo != tee_alg)
			continue;

		mecha->mechanism = conv->ckMechanismType;
		mecha->pParameter = conv->ckParameter;
		if (mecha->pParameter)
			mecha->ulParameterLen = conv->ckParameterLen;
		else
			mecha->ulParameterLen = 0;

		return 0;
	}

	return 1;
}

void run_xtest_tee_test_4217(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	CK_RV rv;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE priv_key_handle = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE pub_key_handle = CK_INVALID_HANDLE;
	uint8_t out[512];
	size_t out_size;
	uint8_t out_enc[512];
	size_t out_enc_size;
	size_t n;
	int subcase = 0;
	/* Compute hash through cryp test TA (until SKS supports hashes */
	uint8_t ptx_hash[TEE_MAX_HASH_SIZE];
	size_t ptx_hash_size;
	TEEC_Session crypta_session = { 0 };
	uint32_t ret_orig;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				xtest_teec_open_session(&crypta_session,
							&crypt_user_ta_uuid,
							NULL, &ret_orig)))
		return;

	rv = C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
			   NULL, 0, &session);
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto out;

	for (n = 0; n < ARRAY_SIZE(xtest_ac_cases); n++) {
		CK_MECHANISM mechanism = { 0 };
		const struct xtest_ac_case *tv = xtest_ac_cases + n;
		CK_ATTRIBUTE *rsa_priv_attr;
		size_t rsa_priv_count;

		if (tv->level > level)
			continue;

		/* Fill Cryptoki mechanism structure from OP-TEE native vectors */
	        if (tee_alg2ckmt(tv->algo, &mechanism)) {
			unsigned int algo = TEE_ALG_GET_MAIN_ALG(tv->algo);

			Do_ADBG_Log("Skip test case %d algo 0x%x line %d (%s)",
				     (int)n, (unsigned int)tv->algo,
				     (int)tv->line,
				     algo == TEE_MAIN_ALGO_RSA ? "RSA" :
				     algo == TEE_MAIN_ALGO_DSA ? "DSA" :
				     algo == TEE_MAIN_ALGO_DH ? "DH" :
				     algo == TEE_MAIN_ALGO_ECDSA ? "ECDSA" :
				     algo == TEE_MAIN_ALGO_ECDH ? "ECDH" :
				     "???");
			continue;
		}

		if (TEE_ALG_GET_MAIN_ALG(tv->algo) == TEE_MAIN_ALGO_RSA &&
		    tv->params.rsa.salt_len > 0) {
			CK_VOID_PTR ptr = mechanism.pParameter;

			((CK_RSA_PKCS_PSS_PARAMS_PTR)ptr)->sLen =
				tv->params.rsa.salt_len;
		}

		Do_ADBG_BeginSubCase(c, "Asym Crypto case %d algo 0x%x line %d (%s)",
				     (int)n, (unsigned int)tv->algo,
				     (int)tv->line,
				     ckm2str(mechanism.mechanism));
		subcase = 1;

		/*
		 * When signing or verifying we're working with the hash of
		 * the payload.
		 */
		if (tv->mode == TEE_MODE_VERIFY || tv->mode == TEE_MODE_SIGN) {
			uint32_t hash_algo;
			TEE_OperationHandle op = TEE_HANDLE_NULL;

			if (TEE_ALG_GET_MAIN_ALG(tv->algo) ==
							TEE_MAIN_ALGO_ECDSA)
				hash_algo = TEE_ALG_SHA1;
#if defined(CFG_CRYPTO_RSASSA_NA1)
			else if (tv->algo == TEE_ALG_RSASSA_PKCS1_V1_5)
				hash_algo = TEE_ALG_SHA256;
#endif
			else
				hash_algo = TEE_ALG_HASH_ALGO(
					TEE_ALG_GET_DIGEST_HASH(tv->algo));

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_allocate_operation(c,
							&crypta_session, &op,
							hash_algo,
							TEE_MODE_DIGEST, 0)))
				goto out;

			ptx_hash_size = sizeof(ptx_hash);
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_digest_do_final(c, &crypta_session,
							op, tv->ptx,
							tv->ptx_len, ptx_hash,
							&ptx_hash_size)))
				goto out;

			/*
			 * When we use DSA algorithms, the size of the hash we
			 * consider equals the min between the size of the
			 * "subprime" in the key and the size of the hash
			 */
			if (TEE_ALG_GET_MAIN_ALG(tv->algo) ==
			    TEE_MAIN_ALGO_DSA) {
				if (tv->params.dsa.sub_prime_len <=
				    ptx_hash_size)
					ptx_hash_size =
						tv->params.dsa.sub_prime_len;
			}

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_operation(c, &crypta_session,
							    op)))
				goto out;
		}


		/* Collect key attributes and create key */
		switch (TEE_ALG_GET_MAIN_ALG(tv->algo)) {
		case TEE_MAIN_ALGO_RSA:

			if (tv->params.rsa.prime1_len) {
				rsa_priv_attr = &rsa_key_priv_attr[0];
				rsa_priv_count = ARRAY_SIZE(rsa_key_priv_attr);
			} else {
				rsa_priv_attr = &rsa_key_priv_attr2[0];
				rsa_priv_count = ARRAY_SIZE(rsa_key_priv_attr2);
			}

			if (SET_CK_ATTR(rsa_key_pub_attr, CKA_MODULUS,
					tv->params.rsa.modulus,
					tv->params.rsa.modulus_len) ||
			    SET_CK_ATTR(rsa_key_pub_attr, CKA_PUBLIC_EXPONENT,
					tv->params.rsa.pub_exp,
					tv->params.rsa.pub_exp_len)) {
				Do_ADBG_Log("Invalid test setup");
				ADBG_EXPECT_TRUE(c, false);
				goto out;
			}

			rv = C_CreateObject(session, rsa_key_pub_attr,
					    ARRAY_SIZE(rsa_key_pub_attr),
					    &pub_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			if (set_ck_attr(rsa_priv_attr, rsa_priv_count,
					CKA_MODULUS,
					(void *)tv->params.rsa.modulus,
					tv->params.rsa.modulus_len) ||
			    set_ck_attr(rsa_priv_attr, rsa_priv_count,
					CKA_PUBLIC_EXPONENT,
					(void *)tv->params.rsa.pub_exp,
					tv->params.rsa.pub_exp_len) ||
			    set_ck_attr(rsa_priv_attr, rsa_priv_count,
					CKA_PRIVATE_EXPONENT,
					(void *)tv->params.rsa.priv_exp,
					tv->params.rsa.priv_exp_len)) {
				Do_ADBG_Log("Invalid test setup");
				ADBG_EXPECT_TRUE(c, false);
				goto out;
			}

			if (tv->params.rsa.prime1_len != 0 &&
			    (set_ck_attr(rsa_priv_attr, rsa_priv_count,
					 CKA_PRIME_1,
					 (void *)tv->params.rsa.prime1,
					 tv->params.rsa.prime1_len) ||
			     set_ck_attr(rsa_priv_attr, rsa_priv_count,
					 CKA_PRIME_2,
					 (void *)tv->params.rsa.prime2,
					 tv->params.rsa.prime2_len) ||
			     set_ck_attr(rsa_priv_attr, rsa_priv_count,
					 CKA_EXPONENT_1,
					 (void *)tv->params.rsa.exp1,
					 tv->params.rsa.exp1_len) ||
			     set_ck_attr(rsa_priv_attr, rsa_priv_count,
					 CKA_EXPONENT_2,
					 (void *)tv->params.rsa.exp2,
					 tv->params.rsa.exp2_len) ||
			     set_ck_attr(rsa_priv_attr, rsa_priv_count,
					 CKA_COEFFICIENT,
					 (void *)tv->params.rsa.coeff,
					 tv->params.rsa.coeff_len))) {
				Do_ADBG_Log("Invalid test setup");
				ADBG_EXPECT_TRUE(c, false);
				goto out;
			}

			rv = C_CreateObject(session, rsa_priv_attr,
					    rsa_priv_count, &priv_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			break;

		case TEE_MAIN_ALGO_ECDSA:
			ck_ec_params_attr_from_tee_algo(
					cktest_ec_key_priv_attr,
					ARRAY_SIZE(cktest_ec_key_priv_attr),
					tv->algo);

			SET_CK_ATTR(cktest_ec_key_priv_attr, CKA_VALUE,
					tv->params.ecdsa.private,
					tv->params.ecdsa.private_len);

			SET_CK_ATTR(cktest_ec_key_priv_attr, CKA_VENDOR_EC_POINT_X,
				       tv->params.ecdsa.public_x,
				       tv->params.ecdsa.public_x_len);
			SET_CK_ATTR(cktest_ec_key_priv_attr, CKA_VENDOR_EC_POINT_Y,
				       tv->params.ecdsa.public_y,
				       tv->params.ecdsa.public_y_len);

			rv = C_CreateObject(session, cktest_ec_key_priv_attr,
					    ARRAY_SIZE(cktest_ec_key_priv_attr),
					    &priv_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			ck_ec_params_attr_from_tee_algo(
					cktest_ec_key_pub_attr,
					ARRAY_SIZE(cktest_ec_key_pub_attr),
					tv->algo);

			SET_CK_ATTR(cktest_ec_key_pub_attr, CKA_VENDOR_EC_POINT_X,
				       tv->params.ecdsa.public_x,
				       tv->params.ecdsa.public_x_len);
			SET_CK_ATTR(cktest_ec_key_pub_attr, CKA_VENDOR_EC_POINT_Y,
				       tv->params.ecdsa.public_y,
				       tv->params.ecdsa.public_y_len);

			rv = C_CreateObject(session, cktest_ec_key_pub_attr,
					    ARRAY_SIZE(cktest_ec_key_pub_attr),
					    &pub_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			break;

		default:
			break;
		}

		memset(out, 0, sizeof(out));
		memset(out_enc, 0, sizeof(out_enc));

		switch (tv->mode) {
		case TEE_MODE_ENCRYPT:
			rv = C_EncryptInit(session, &mechanism, pub_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			out_enc_size = sizeof(out_enc);
			rv = C_Encrypt(session,
					(CK_VOID_PTR)tv->ptx, tv->ptx_len,
					(CK_VOID_PTR)out_enc,
					(CK_ULONG_PTR)&out_enc_size);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			/*
			 * A PS which is random is added when formatting the
			 * message internally of the algorithm so we can't
			 * verify against precomputed values, instead we use the
			 * decrypt operation to see that output is correct.
			 */
			rv = C_DecryptInit(session, &mechanism, priv_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			out_size = sizeof(out);
			rv = C_Decrypt(session,
					(CK_VOID_PTR)out_enc, out_enc_size,
					out, (CK_ULONG_PTR)&out_size);
			if (!ADBG_EXPECT_CK_OK(c, rv))
		                goto out;

			ADBG_EXPECT_BUFFER(c, tv->ptx, tv->ptx_len,
					   out, out_size);
			break;

		case TEE_MODE_DECRYPT:
			rv = C_DecryptInit(session, &mechanism, priv_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
		                goto out;

			out_size = sizeof(out);
			rv = C_Decrypt(session,
					(CK_VOID_PTR)tv->ctx, tv->ctx_len,
					out, (CK_ULONG_PTR) &out_size);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			ADBG_EXPECT_BUFFER(c, tv->ptx, tv->ptx_len,
					   out, out_size);
			break;

		case TEE_MODE_SIGN:
			rv = C_SignInit(session, &mechanism, priv_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			out_size = sizeof(out);
			rv = C_Sign(session,
				    (CK_VOID_PTR)ptx_hash,
				    (CK_ULONG)ptx_hash_size,
				    (CK_VOID_PTR)out,
				    (CK_ULONG_PTR)&out_size);
			if (!ADBG_EXPECT_CK_OK(c, rv))
		                goto out;

			/*
			 * The salt or K is random so we can't verify
			 * signing against precomputed values, instead
			 * we use the verify operation to see that
			 * output is correct.
			 */
			if (TEE_ALG_GET_CHAIN_MODE(tv->algo) ==
						TEE_CHAIN_MODE_PKCS1_PSS_MGF1 ||
			    tv->algo == TEE_ALG_DSA_SHA1 ||
			    tv->algo == TEE_ALG_DSA_SHA224 ||
			    tv->algo == TEE_ALG_DSA_SHA256 ||
			    TEE_ALG_GET_MAIN_ALG(tv->algo) ==
						TEE_MAIN_ALGO_ECDSA) {

				rv = C_VerifyInit(session, &mechanism,
						  pub_key_handle);
				if (!ADBG_EXPECT_CK_OK(c, rv))
					goto out;

				rv = C_Verify(session,
						(CK_BYTE_PTR)ptx_hash,
						(CK_ULONG)ptx_hash_size,
						(CK_BYTE_PTR)out,
						(CK_ULONG)out_size);
				if (!ADBG_EXPECT_CK_OK(c, rv))
					goto out;
			} else {
				ADBG_EXPECT_BUFFER(c, tv->ctx, tv->ctx_len,
						   out, out_size);
			}
			break;

	        case TEE_MODE_VERIFY:
			rv = C_VerifyInit(session, &mechanism, pub_key_handle);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			rv = C_Verify(session, (CK_BYTE_PTR)&ptx_hash[0],
						(CK_ULONG)ptx_hash_size,
						(CK_BYTE_PTR)tv->ctx,
						(CK_ULONG)tv->ctx_len);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto out;

			break;

		default:
			break;
		}

		rv = C_DestroyObject(session, priv_key_handle);
		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		priv_key_handle = CK_INVALID_HANDLE;

		rv = C_DestroyObject(session, pub_key_handle);
		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		pub_key_handle = CK_INVALID_HANDLE;

		Do_ADBG_EndSubCase(c, NULL);
		subcase = 0;
	}
out:
	if (subcase)
		Do_ADBG_EndSubCase(c, NULL);

	TEEC_CloseSession(&crypta_session);

	rv = C_CloseSession(session);
	ADBG_EXPECT_CK_OK(c, rv);
}
#endif

#define KEY_ATTR(x, y) { #x, (x), y }

struct key_attrs {
	const char *name;
	uint32_t attr;
	/*
	 * When keysize_check != 0: size of attribute is checked
	 * Expected value is key_size bits except for DH in which case it is
	 * the value of keysize_check.
	 */
	uint32_t keysize_check;
};

static bool test_keygen_attributes(ADBG_Case_t *c, TEEC_Session *s,
				   TEE_ObjectHandle key, uint32_t key_size,
				   struct key_attrs *attrs, size_t num_attrs)
{
	uint8_t out[2048];
	size_t out_size;
	size_t n;
	size_t m;

	for (m = 0; m < num_attrs; m++) {
		if ((attrs[m].attr & TEE_ATTR_BIT_VALUE) == 0) {
			out_size = sizeof(out);
			memset(out, 0, sizeof(out));
			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_get_object_buffer_attribute(c, s,
					key, attrs[m].attr, out, &out_size)))
				return false;

			if (attrs[m].keysize_check)
				ADBG_EXPECT_COMPARE_UNSIGNED(c, out_size, ==,
							     key_size / 8);

			if (out_size > 0) {
				/* Check that buffer isn't all zeroes */
				for (n = 0; n < out_size; n++)
					if (out[n] != 0)
						break;
				if (!ADBG_EXPECT_COMPARE_UNSIGNED(c, n, <,
								  out_size))
					return false;
			}
		} else {
			uint32_t a;
			uint32_t b;

			if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_get_object_value_attribute(c, s, key,
					attrs[m].attr, &a, &b)))
				return false;
		}
	}
	return true;
}

static bool test_secret_value(ADBG_Case_t *c, TEEC_Session *s,
			      TEE_ObjectHandle key, uint32_t key_size)
{
	const struct key_attrs attrs[] = {
		KEY_ATTR(TEE_ATTR_SECRET_VALUE, true),
	};

	return test_keygen_attributes(c, s, key, key_size,
				      (struct key_attrs *)&attrs,
				      ARRAY_SIZE(attrs));
}


static bool test_rsa_key_pair(ADBG_Case_t *c, TEEC_Session *s,
			      TEE_ObjectHandle key, uint32_t key_size)
{
	const struct key_attrs attrs[] = {
		KEY_ATTR(TEE_ATTR_RSA_MODULUS, true),
		KEY_ATTR(TEE_ATTR_RSA_PUBLIC_EXPONENT, false),
		KEY_ATTR(TEE_ATTR_RSA_PRIVATE_EXPONENT, false),
		KEY_ATTR(TEE_ATTR_RSA_PRIME1, false),
		KEY_ATTR(TEE_ATTR_RSA_PRIME2, false),
		KEY_ATTR(TEE_ATTR_RSA_EXPONENT1, false),
		KEY_ATTR(TEE_ATTR_RSA_EXPONENT2, false),
		KEY_ATTR(TEE_ATTR_RSA_COEFFICIENT, false),
	};

	return test_keygen_attributes(c, s, key, key_size,
				      (struct key_attrs *)&attrs,
				      ARRAY_SIZE(attrs));
}

static bool test_ecc_key_pair(ADBG_Case_t *c, TEEC_Session *s,
			      TEE_ObjectHandle key, uint32_t key_size)
{
	const struct key_attrs attrs[] = {
		KEY_ATTR(TEE_ATTR_ECC_PRIVATE_VALUE, false),
		KEY_ATTR(TEE_ATTR_ECC_PUBLIC_VALUE_X , false),
		KEY_ATTR(TEE_ATTR_ECC_PUBLIC_VALUE_Y , false),
		/* KEY_ATTR(TEE_ATTR_ECC_CURVE, false), - do not test */
	};

	return test_keygen_attributes(c, s, key, key_size,
				      (struct key_attrs *)&attrs,
				      ARRAY_SIZE(attrs));
}

static bool test_dh_key_pair(ADBG_Case_t *c, TEEC_Session *s,
			     TEE_ObjectHandle key, uint32_t check_keysize)
{
	const struct key_attrs attrs[] = {
		KEY_ATTR(TEE_ATTR_DH_PRIME, false),
		KEY_ATTR(TEE_ATTR_DH_BASE, false),
		KEY_ATTR(TEE_ATTR_DH_PUBLIC_VALUE, false),
		KEY_ATTR(TEE_ATTR_DH_PRIVATE_VALUE, check_keysize),
		KEY_ATTR(TEE_ATTR_DH_X_BITS, false),
	};

	return test_keygen_attributes(c, s, key, check_keysize,
				      (struct key_attrs *)&attrs,
				      ARRAY_SIZE(attrs));
}

static bool test_dsa_key_pair(ADBG_Case_t *c, TEEC_Session *s,
			      TEE_ObjectHandle key, uint32_t key_size)
{
	const struct key_attrs attrs[] = {
		KEY_ATTR(TEE_ATTR_DSA_PRIME, true),
		KEY_ATTR(TEE_ATTR_DSA_SUBPRIME, false),
		KEY_ATTR(TEE_ATTR_DSA_BASE, false),
		KEY_ATTR(TEE_ATTR_DSA_PUBLIC_VALUE, false),
		KEY_ATTR(TEE_ATTR_DSA_PRIVATE_VALUE, false),
	};

	return test_keygen_attributes(c, s, key, key_size,
				      (struct key_attrs *)&attrs,
				      ARRAY_SIZE(attrs));
}

static bool generate_and_test_key(ADBG_Case_t *c, TEEC_Session *s,
				  uint32_t key_type, uint32_t check_keysize,
				  uint32_t key_size,
				  TEE_Attribute *params, size_t param_count)
{
	TEE_ObjectHandle key;
	bool ret_val = true;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_transient_object(c, s, key_type, key_size,
						       &key)))
		return false;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_generate_key(c, s, key, key_size, params,
					  param_count)))
		return false;

	switch (key_type) {
	case TEE_TYPE_DES:
	case TEE_TYPE_DES3:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_secret_value(c, s, key,
						  key_size + key_size / 7));
		break;
	case TEE_TYPE_AES:
	case TEE_TYPE_HMAC_MD5:
	case TEE_TYPE_HMAC_SHA1:
	case TEE_TYPE_HMAC_SHA224:
	case TEE_TYPE_HMAC_SHA256:
	case TEE_TYPE_HMAC_SHA384:
	case TEE_TYPE_HMAC_SHA512:
	case TEE_TYPE_GENERIC_SECRET:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_secret_value(c, s, key, key_size));
		break;

	case TEE_TYPE_RSA_KEYPAIR:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_rsa_key_pair(c, s, key, key_size));
		break;

	case TEE_TYPE_ECDSA_KEYPAIR:
	case TEE_TYPE_ECDH_KEYPAIR:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_ecc_key_pair(c, s, key, key_size));
		break;

	case TEE_TYPE_DH_KEYPAIR:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_dh_key_pair(c, s, key, check_keysize));
		break;

	case TEE_TYPE_DSA_KEYPAIR:
		ret_val = ADBG_EXPECT_TRUE(c,
				test_dsa_key_pair(c, s, key, key_size));
		break;

	default:
		ret_val = false;
		break;
	}

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_free_transient_object(c, s, key)))
		return false;

	return ret_val;
}

#ifdef CFG_SECURE_KEY_SERVICES
static bool cktest_generate_and_test_key(ADBG_Case_t *c,
					 CK_SESSION_HANDLE session,
					 CK_MECHANISM *ck_mechanism,
					 CK_ATTRIBUTE *ck_attrs, CK_ULONG ck_count,
					 CK_ATTRIBUTE *ck_attrs2, CK_ULONG ck_count2,
					 uint32_t __unused check_keysize,
					 uint32_t __unused key_size,
					 void __unused *rsv,
					 size_t __unused rsv_sz)
{
	CK_ATTRIBUTE cktest_findobj_local[] = {
		{ CKA_LOCAL, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	};
	CK_RV rv;
	CK_OBJECT_HANDLE obj_hdl;
	CK_OBJECT_HANDLE obj_hdl2;
	CK_OBJECT_HANDLE obj_hdl3;
	CK_ULONG count = 1;

	if (ck_count2) {
		rv = C_GenerateKeyPair(session, ck_mechanism,
					ck_attrs, ck_count,
					ck_attrs2, ck_count2,
					&obj_hdl, &obj_hdl2);
	} else {
		rv = C_GenerateKey(session, ck_mechanism,
				   ck_attrs, ck_count, &obj_hdl);
	}
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto bail;

	/*
	 * Weak test: check a local object is created and matches our handle.
	 * TODO: get key attribute data and check the size.
	 */
	rv = C_FindObjectsInit(session, cktest_findobj_local,
				ARRAY_SIZE(cktest_findobj_local));
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto bail;

	obj_hdl3 = CK_INVALID_HANDLE;
	while (1) {
		rv = C_FindObjects(session, &obj_hdl3, 1, &count);
		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto bail;


		if (!count || obj_hdl3 == obj_hdl)
			break;
	}

	if (!ADBG_EXPECT_COMPARE_UNSIGNED(c, obj_hdl3, ==, obj_hdl))
		rv = CKR_GENERAL_ERROR;

	ADBG_EXPECT_CK_OK(c, C_FindObjectsFinal(session));
	ADBG_EXPECT_CK_OK(c, C_DestroyObject(session, obj_hdl));

	if (ck_count2) {
		obj_hdl3 = CK_INVALID_HANDLE;

		rv = C_FindObjectsInit(session, cktest_findobj_local,
					ARRAY_SIZE(cktest_findobj_local));
		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto bail;

		while (1) {
			rv = C_FindObjects(session, &obj_hdl3, 1, &count);
			if (!ADBG_EXPECT_CK_OK(c, rv))
				goto bail;

			if (!count || obj_hdl3 == obj_hdl2)
				break;
		}

		if (!ADBG_EXPECT_COMPARE_UNSIGNED(c, obj_hdl3, ==, obj_hdl2))
			rv = CKR_GENERAL_ERROR;

		ADBG_EXPECT_CK_OK(c, C_FindObjectsFinal(session));
		ADBG_EXPECT_CK_OK(c, C_DestroyObject(session, obj_hdl2));
	}

bail:
	return rv == CKR_OK;
}
#endif

static const struct {
	unsigned level;
	const char *name;
	uint32_t key_type;
	uint32_t quanta;
	uint32_t min_size;
	uint32_t max_size;
} keygen_noparams_key_types[] = {
	{ 0, "AES", TEE_TYPE_AES, 64, 128,
	  256 /* valid sizes 128, 192, 256 */ },
	{ 0, "DES", TEE_TYPE_DES, 56, 56, 56 /* valid size 56 */ },
	{ 0, "DES3", TEE_TYPE_DES3, 56, 112,
	  168 /* valid sizes 112, 168 */ },
	{ 0, "HMAC-MD5", TEE_TYPE_HMAC_MD5, 8, 64, 512 },
	{ 0, "HMAC-SHA1", TEE_TYPE_HMAC_SHA1, 8, 80, 512 },
	{ 0, "HMAC-SHA224", TEE_TYPE_HMAC_SHA224, 8, 112, 512 },
	{ 0, "HMAC-SHA256", TEE_TYPE_HMAC_SHA256, 8, 192, 1024 },
	{ 0, "HMAC-SHA384", TEE_TYPE_HMAC_SHA384, 8, 256, 1024 },
	{ 0, "HMAC-SHA512", TEE_TYPE_HMAC_SHA512, 8, 256, 1024 },
	{ 0, "Generic secret", TEE_TYPE_GENERIC_SECRET, 8, 128, 4096 },
	{ 1, "RSA-2048", TEE_TYPE_RSA_KEYPAIR, 1, 2048, 2048 },

	/* New tests added to check non-regression of issue #5398 */
	{ 0, "RSA-256", TEE_TYPE_RSA_KEYPAIR, 1, 256, 256 },
	{ 1, "RSA-384", TEE_TYPE_RSA_KEYPAIR, 1, 384, 384 },
	{ 1, "RSA-512", TEE_TYPE_RSA_KEYPAIR, 1, 512, 512 },
	{ 1, "RSA-640", TEE_TYPE_RSA_KEYPAIR, 1, 640, 640 },
	{ 1, "RSA-768", TEE_TYPE_RSA_KEYPAIR, 1, 768, 768 },
	{ 1, "RSA-896", TEE_TYPE_RSA_KEYPAIR, 1, 896, 896 },
	{ 1, "RSA-1024", TEE_TYPE_RSA_KEYPAIR, 1, 1024, 1024 },
};

struct key_types_noparam {
	unsigned level;
	const char *name;
	uint32_t key_type;
	uint32_t quanta;
	uint32_t min_size;
	uint32_t max_size;
};

static void keygen_noparams(ADBG_Case_t *c, TEEC_Session *session,
			    const struct key_types_noparam *key_types,
			    size_t num_key_types)
{
	size_t n;
	uint32_t key_size;

	for (n = 0; n < num_key_types; n++) {
		uint32_t min_size = key_types[n].min_size;
		uint32_t max_size = key_types[n].max_size;
		uint32_t quanta = key_types[n].quanta;

		if (key_types[n].level > level)
			continue;

		Do_ADBG_BeginSubCase(c, "Generate %s key", key_types[n].name);

		for (key_size = min_size; key_size <= max_size;
		     key_size += quanta) {
			if (!ADBG_EXPECT_TRUE(c,
				generate_and_test_key(c, session, key_types
					[n].key_type, 1, key_size, NULL, 0)))
				break;
		}

		Do_ADBG_EndSubCase(c, "Generate %s key", key_types[n].name);
	}
}

static void xtest_tee_test_4007_symmetric(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	static const struct key_types_noparam key_types[] = {
		{ 0, "AES", TEE_TYPE_AES, 64, 128,
		  256 /* valid sizes 128, 192, 256 */ },
		{ 0, "DES", TEE_TYPE_DES, 56, 56, 56 /* valid size 56 */ },
		{ 0, "DES3", TEE_TYPE_DES3, 56, 112,
		  168 /* valid sizes 112, 168 */ },
		{ 0, "HMAC-MD5", TEE_TYPE_HMAC_MD5, 8, 64, 512 },
		{ 0, "HMAC-SHA1", TEE_TYPE_HMAC_SHA1, 8, 80, 512 },
		{ 0, "HMAC-SHA224", TEE_TYPE_HMAC_SHA224, 8, 112, 512 },
		{ 0, "HMAC-SHA256", TEE_TYPE_HMAC_SHA256, 8, 192, 1024 },
		{ 0, "HMAC-SHA384", TEE_TYPE_HMAC_SHA384, 8, 256, 1024 },
		{ 0, "HMAC-SHA512", TEE_TYPE_HMAC_SHA512, 8, 256, 1024 },
		{ 0, "Generic secret", TEE_TYPE_GENERIC_SECRET, 8, 128, 4096 },
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	keygen_noparams(c, &session, key_types, ARRAY_SIZE(key_types));

	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4007_symmetric, xtest_tee_test_4007_symmetric,
		"Test TEE Internal API Generate Symmetric key");

static void xtest_tee_test_4007_rsa(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	static const struct key_types_noparam key_types[] = {
		{ 0, "RSA-256", TEE_TYPE_RSA_KEYPAIR, 1, 256, 256 },
		{ 1, "RSA-384", TEE_TYPE_RSA_KEYPAIR, 1, 384, 384 },
		{ 1, "RSA-512", TEE_TYPE_RSA_KEYPAIR, 1, 512, 512 },
		{ 1, "RSA-640", TEE_TYPE_RSA_KEYPAIR, 1, 640, 640 },
		{ 1, "RSA-768", TEE_TYPE_RSA_KEYPAIR, 1, 768, 768 },
		{ 1, "RSA-896", TEE_TYPE_RSA_KEYPAIR, 1, 896, 896 },
		{ 1, "RSA-1024", TEE_TYPE_RSA_KEYPAIR, 1, 1024, 1024 },
		{ 1, "RSA-2048", TEE_TYPE_RSA_KEYPAIR, 1, 2048, 2048 },
		{ 1, "RSA-3072", TEE_TYPE_RSA_KEYPAIR, 1, 3072, 3072 },
		{ 1, "RSA-4096", TEE_TYPE_RSA_KEYPAIR, 1, 4096, 4096 },
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	keygen_noparams(c, &session, key_types, ARRAY_SIZE(key_types));

	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4007_rsa, xtest_tee_test_4007_rsa,
		"Test TEE Internal API Generate RSA key");

#ifdef CFG_SECURE_KEY_SERVICES

/* Valid template to generate a generic secret */
static CK_ATTRIBUTE cktest_keygen_noparams_symkey[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},
			sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE, &(CK_KEY_TYPE){0}, sizeof(CK_KEY_TYPE) },
	{ CKA_ENCRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_VALUE_LEN, &(CK_ULONG){16}, sizeof(CK_ULONG) },
};
static CK_ATTRIBUTE cktest_keygen_noparams_rsa_pub[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PUBLIC_KEY},
			sizeof(CK_OBJECT_CLASS) },
	{ CKA_KEY_TYPE, &(CK_KEY_TYPE){CKK_RSA}, sizeof(CK_KEY_TYPE) },
	{ CKA_VERIFY, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_ENCRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_MODULUS_BITS, &(CK_ULONG){0}, sizeof(CK_ULONG) },
};
static CK_ATTRIBUTE cktest_keygen_noparams_rsa_priv[] = {
	{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_PRIVATE_KEY},
			sizeof(CK_OBJECT_CLASS) },
	/* Intentionally not key type: libsks will guess the key type */
	{ CKA_SIGN, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
	{ CKA_DECRYPT, &(CK_BBOOL){CK_TRUE}, sizeof(CK_BBOOL) },
};

static void cktest_keygen_noparams(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	size_t n;
	uint32_t key_size;
	CK_RV rv;
	CK_SESSION_HANDLE session;

	for (n = 0; n < ARRAY_SIZE(keygen_noparams_key_types); n++) {
		uint32_t min_size = keygen_noparams_key_types[n].min_size;
		uint32_t max_size = keygen_noparams_key_types[n].max_size;
		uint32_t quanta = keygen_noparams_key_types[n].quanta;
		CK_MECHANISM ck_mecha = { 0 };
		CK_ATTRIBUTE *ck_attrs;
		CK_ULONG ck_count;
		CK_ATTRIBUTE *ck_attrs2;
		CK_ULONG ck_count2;
		CK_ULONG ck_key_size;
		CK_ULONG ck_key_type;
		CK_FLAGS session_flags = CKF_SERIAL_SESSION;

		if (keygen_noparams_key_types[n].level > level)
			continue;

		switch (keygen_noparams_key_types[n].key_type) {
		case TEE_TYPE_AES:
			ck_key_type = CKK_AES;
			ck_mecha.mechanism = CKM_AES_KEY_GEN;
			ck_attrs = cktest_keygen_noparams_symkey;
			ck_count = ARRAY_SIZE(cktest_keygen_noparams_symkey);
			ck_attrs2 = NULL;
			ck_count2 = 0;
			break;

		case TEE_TYPE_DES:
		case TEE_TYPE_DES3:
			continue;

		case TEE_TYPE_HMAC_MD5:
		case TEE_TYPE_HMAC_SHA1:
		case TEE_TYPE_HMAC_SHA224:
		case TEE_TYPE_HMAC_SHA256:
		case TEE_TYPE_HMAC_SHA384:
		case TEE_TYPE_HMAC_SHA512:
		case TEE_TYPE_GENERIC_SECRET:
			ck_key_type = CKK_GENERIC_SECRET;
			ck_mecha.mechanism = CKM_GENERIC_SECRET_KEY_GEN;
			ck_attrs = cktest_keygen_noparams_symkey;
			ck_count = ARRAY_SIZE(cktest_keygen_noparams_symkey);
			ck_attrs2 = NULL;
			ck_count2 = 0;
			break;

		case TEE_TYPE_RSA_KEYPAIR:
			ck_key_type = CKK_RSA;
			ck_mecha.mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
			ck_attrs = cktest_keygen_noparams_rsa_pub;
			ck_count = ARRAY_SIZE(cktest_keygen_noparams_rsa_pub);
			ck_attrs2 = cktest_keygen_noparams_rsa_priv;
			ck_count2 = ARRAY_SIZE(cktest_keygen_noparams_rsa_priv);
			break;

		default:
			continue;
		}

		Do_ADBG_BeginSubCase(c, "Generate %s key",
					keygen_noparams_key_types[n].name);

		rv = C_OpenSession(slot, session_flags, NULL, 0, &session);
		if (!ADBG_EXPECT_CK_OK(c, rv))
			return;

		/* libsks guesses the key type in RSA or EC cases */
		if (ck_key_type != CKK_RSA) {
			if (set_ck_attr(ck_attrs, ck_count, CKA_KEY_TYPE,
					(void **)&ck_key_type,
					sizeof(ck_key_type))) {
				goto broken_test;
			}
		}

		for (key_size = min_size; key_size <= max_size;
		     key_size += quanta) {
			bool r;

			switch (keygen_noparams_key_types[n].key_type) {
			case TEE_TYPE_AES:
			case TEE_TYPE_HMAC_MD5:
			case TEE_TYPE_HMAC_SHA1:
			case TEE_TYPE_HMAC_SHA224:
			case TEE_TYPE_HMAC_SHA256:
			case TEE_TYPE_HMAC_SHA384:
			case TEE_TYPE_HMAC_SHA512:
			case TEE_TYPE_GENERIC_SECRET:
				/* Most PKCS#11 keys are length in bytes */
				if (keygen_noparams_key_types[n].key_type ==
				    TEE_TYPE_GENERIC_SECRET)
					ck_key_size = key_size;
				else
					ck_key_size = key_size / 8;

				if (set_ck_attr(ck_attrs, ck_count, CKA_VALUE_LEN,
						(void *)&ck_key_size,
						sizeof(ck_key_size)))
					goto broken_test;

				break;

			case TEE_TYPE_DES:
			case TEE_TYPE_DES3:
				continue;

			case TEE_TYPE_RSA_KEYPAIR:
				ck_key_size = key_size;
				if (set_ck_attr(ck_attrs, ck_count, CKA_MODULUS_BITS,
						(void *)&ck_key_size,
						sizeof(ck_key_size)))
					goto broken_test;
				break;

			default:
				continue;
			}

			r = cktest_generate_and_test_key(c, session, &ck_mecha,
							 ck_attrs, ck_count,
							 ck_attrs2, ck_count2,
							 1 /*test size*/,
							 key_size, NULL, 0);

			if (!ADBG_EXPECT_TRUE(c, r))
				break;
		}

		C_CloseSession(session);

		Do_ADBG_EndSubCase(c, "Generate %s key",
					keygen_noparams_key_types[n].name);
	}

	return;

broken_test:
	Do_ADBG_Log("Broken test setup for key %s",
			keygen_noparams_key_types[n].name);
	ADBG_EXPECT_TRUE(c, false);

	C_CloseSession(session);

	Do_ADBG_EndSubCase(c, NULL);
}
#endif

static void xtest_tee_test_4007_dh(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	size_t n;
	size_t param_count;
	/*
	 * Note that the key size parameter is not used when creating the keys
	 * but specifying these sizes make it possible to test the expected size
	 * of the private value. This also means that the keysize must match the
	 * size of p or what is specified in private_bits or the equvivalent
	 * size of the subprime parameter.
	 */
	TEE_Attribute params[4];

#define XTEST_DH_GK_DATA(vect) \
	ARRAY(vect ## _p), \
	ARRAY(vect ## _g), \
	&vect ## _private_bits, \
	0, 0
#define XTEST_DH_GK_DATA_SUBPRIME(vect) \
	ARRAY(vect ## _p), \
	ARRAY(vect ## _g), \
	&vect ## _private_bits, \
	ARRAY(vect ## _subprime)
	static const struct {
		unsigned level;
		uint32_t key_size;
		const uint8_t *p;
		size_t p_len;
		const uint8_t *g;
		size_t g_len;
		const uint32_t *private_bits;
		const uint8_t *subprime;
		size_t subprime_len;
	} key_types[] = {
		{ 0, 256, XTEST_DH_GK_DATA(keygen_dh256) },
		{ 0, 320, XTEST_DH_GK_DATA(keygen_dh320) },
		{ 1, 384, XTEST_DH_GK_DATA(keygen_dh384) },
		{ 1, 448, XTEST_DH_GK_DATA(keygen_dh448) },
		{ 1, 512, XTEST_DH_GK_DATA(keygen_dh512) },
		{ 1, 576, XTEST_DH_GK_DATA(keygen_dh576) },
		{ 1, 640, XTEST_DH_GK_DATA(keygen_dh640) },
		{ 1, 704, XTEST_DH_GK_DATA(keygen_dh704) },
		{ 1, 768, XTEST_DH_GK_DATA(keygen_dh768) },
		{ 1, 832, XTEST_DH_GK_DATA(keygen_dh832) },
		{ 1, 896, XTEST_DH_GK_DATA(keygen_dh896) },
		{ 1, 960, XTEST_DH_GK_DATA(keygen_dh960) },
		{ 1, 1024, XTEST_DH_GK_DATA(keygen_dh1024) },
		{ 1, 1088, XTEST_DH_GK_DATA(keygen_dh1088) },
		{ 1, 1152, XTEST_DH_GK_DATA(keygen_dh1152) },
		{ 1, 1216, XTEST_DH_GK_DATA(keygen_dh1216) },
		{ 1, 1280, XTEST_DH_GK_DATA(keygen_dh1280) },
		{ 1, 1344, XTEST_DH_GK_DATA(keygen_dh1344) },
		{ 1, 1408, XTEST_DH_GK_DATA(keygen_dh1408) },
		{ 1, 1472, XTEST_DH_GK_DATA(keygen_dh1472) },
		{ 1, 1536, XTEST_DH_GK_DATA(keygen_dh1536) },
		{ 1, 1600, XTEST_DH_GK_DATA(keygen_dh1600) },
		{ 1, 1664, XTEST_DH_GK_DATA(keygen_dh1664) },
		{ 1, 1728, XTEST_DH_GK_DATA(keygen_dh1728) },
		{ 1, 1792, XTEST_DH_GK_DATA(keygen_dh1792) },
		{ 1, 1856, XTEST_DH_GK_DATA(keygen_dh1856) },
		{ 1, 1920, XTEST_DH_GK_DATA(keygen_dh1920) },
		{ 1, 1984, XTEST_DH_GK_DATA(keygen_dh1984) },
		{ 1, 2048, XTEST_DH_GK_DATA(keygen_dh2048) },
		{ 1, 2048, XTEST_DH_GK_DATA_SUBPRIME(keygen_dh2048_subprime) }
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(key_types); n++) {
		if (key_types[n].level > level)
			continue;

		Do_ADBG_BeginSubCase(c,
				     "Generate DH key %d bits - Private bits = %d",
				     key_types[n].key_size,
				     *key_types[n].private_bits);
		param_count = 0;

		xtest_add_attr(&param_count, params,
			       TEE_ATTR_DH_PRIME,
			       key_types[n].p, key_types[n].p_len);

		xtest_add_attr(&param_count, params, TEE_ATTR_DH_BASE,
			       key_types[n].g, key_types[n].g_len);

		if (key_types[n].private_bits != 0) {
			params[param_count].attributeID = TEE_ATTR_DH_X_BITS;

			params[param_count].content.value.a =
				*key_types[n].private_bits;

			params[param_count].content.value.b = 0;
			param_count++;
		}

		if (key_types[n].subprime != 0) {
			xtest_add_attr(&param_count, params,
				       TEE_ATTR_DH_SUBPRIME,
				       key_types[n].subprime,
				       key_types[n].subprime_len);
		}

		if (!ADBG_EXPECT_TRUE(c,
			generate_and_test_key(c, &session, TEE_TYPE_DH_KEYPAIR,
				*key_types[n].private_bits,
				key_types[n]. key_size, params, param_count)))
			break;

		Do_ADBG_EndSubCase(c,
				   "Generate DH key %d bits - Private bits = %d",
				   key_types[n].key_size,
				   *key_types[n].private_bits);
	}

	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4007_dh, xtest_tee_test_4007_dh,
		"Test TEE Internal API Generate DH key");

static void xtest_tee_test_4007_dsa(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	size_t n;
	size_t param_count;
	TEE_Attribute params[4];

#define XTEST_DSA_GK_DATA(vect) \
	ARRAY(vect ## _p), \
	ARRAY(vect ## _g), \
	ARRAY(vect ## _q)
	static const struct {
		unsigned level;
		uint32_t key_size;
		const uint8_t *prime;
		size_t prime_len;
		const uint8_t *base;
		size_t base_len;
		const uint8_t *sub_prime;
		size_t sub_prime_len;
	} key_types[] = {
		{ 1, 1024, XTEST_DSA_GK_DATA(keygen_dsa_test1) },
		{ 0, 512, XTEST_DSA_GK_DATA(keygen_dsa512) },
		{ 1, 576, XTEST_DSA_GK_DATA(keygen_dsa576) },
		{ 1, 640, XTEST_DSA_GK_DATA(keygen_dsa640) },
		{ 1, 704, XTEST_DSA_GK_DATA(keygen_dsa704) },
		{ 1, 768, XTEST_DSA_GK_DATA(keygen_dsa768) },
		{ 1, 832, XTEST_DSA_GK_DATA(keygen_dsa832) },
		{ 1, 896, XTEST_DSA_GK_DATA(keygen_dsa896) },
		{ 1, 960, XTEST_DSA_GK_DATA(keygen_dsa960) },
		{ 1, 1024, XTEST_DSA_GK_DATA(keygen_dsa1024) },
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(key_types); n++) {
		if (key_types[n].level > level)
			continue;

		Do_ADBG_BeginSubCase(c, "Generate DSA key %d bits",
				     key_types[n].key_size);
		param_count = 0;


		xtest_add_attr(&param_count, params, TEE_ATTR_DSA_PRIME,
			       key_types[n].prime, key_types[n].prime_len);

		xtest_add_attr(&param_count, params, TEE_ATTR_DSA_SUBPRIME,
			       key_types[n].sub_prime,
			       key_types[n].sub_prime_len);

		xtest_add_attr(&param_count, params, TEE_ATTR_DSA_BASE,
			       key_types[n].base, key_types[n].base_len);

		if (!ADBG_EXPECT_TRUE(c,
			generate_and_test_key(c, &session, TEE_TYPE_DSA_KEYPAIR,
				1, key_types[n]. key_size, params,
				param_count)))
			break;

		Do_ADBG_EndSubCase(c, "Generate DSA key %d bits",
				   key_types[n].key_size);
	}

	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4007_dsa, xtest_tee_test_4007_dsa,
		"Test TEE Internal API Generate DSA key");

static void xtest_tee_test_4007_ecc(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	size_t n;
	size_t param_count;
	TEE_Attribute params[4];

	static const struct {
		unsigned level;
		const char *name;
		uint32_t algo;
		uint32_t curve;
		uint32_t key_size;
	} key_types[] = {
	/* ECDSA */
	{ 0, "ECDSA-192", TEE_TYPE_ECDSA_KEYPAIR, TEE_ECC_CURVE_NIST_P192,
		192 },
	{ 1, "ECDSA-224", TEE_TYPE_ECDSA_KEYPAIR, TEE_ECC_CURVE_NIST_P224,
		224 },
	{ 1, "ECDSA-256", TEE_TYPE_ECDSA_KEYPAIR, TEE_ECC_CURVE_NIST_P256,
		256 },
	{ 1, "ECDSA-384", TEE_TYPE_ECDSA_KEYPAIR, TEE_ECC_CURVE_NIST_P384,
		384 },
	{ 1, "ECDSA-521", TEE_TYPE_ECDSA_KEYPAIR, TEE_ECC_CURVE_NIST_P521,
		521 },

	/* ECDH */
	{ 0, "ECDH-192", TEE_TYPE_ECDH_KEYPAIR, TEE_ECC_CURVE_NIST_P192,
		192 },
	{ 1, "ECDH-224", TEE_TYPE_ECDH_KEYPAIR, TEE_ECC_CURVE_NIST_P224,
		224 },
	{ 1, "ECDH-256", TEE_TYPE_ECDH_KEYPAIR, TEE_ECC_CURVE_NIST_P256,
		256 },
	{ 1, "ECDH-384", TEE_TYPE_ECDH_KEYPAIR, TEE_ECC_CURVE_NIST_P384,
		384 },
	{ 1, "ECDH-521", TEE_TYPE_ECDH_KEYPAIR, TEE_ECC_CURVE_NIST_P521,
		521 },
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (n = 0; n < ARRAY_SIZE(key_types); n++) {
		if (key_types[n].level > level)
			continue;

		Do_ADBG_BeginSubCase(c, "Generate %s", key_types[n].name);
		param_count = 0;

		xtest_add_attr_value(&param_count, params, TEE_ATTR_ECC_CURVE,
			             key_types[n].curve, 0);

		if (!ADBG_EXPECT_TRUE(c,
			generate_and_test_key(c, &session, key_types[n].algo,
				0, key_types[n].key_size, params,
				param_count)))
			break;

		Do_ADBG_EndSubCase(c, "Generate %s", key_types[n].name);
	}

	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4007_ecc, xtest_tee_test_4007_ecc,
		"Test TEE Internal API Generate ECC key");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence is implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4216(ADBG_Case_t *c, CK_SLOT_ID slot);
void run_xtest_tee_test_4216(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	cktest_keygen_noparams(c, slot);

	//cktest_keygen_dh(c, slot);

	//cktest_keygen_dsa(c, slot);

	//cktest_keygen_ecc(c, slot);
}
#endif

static void xtest_tee_test_4008(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	TEE_OperationHandle op;
	TEE_ObjectHandle key_handle;
	TEE_ObjectHandle sv_handle;
	TEE_Attribute params[4];
	size_t param_count = 0;
	uint8_t out[2048];
	size_t out_size;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	Do_ADBG_BeginSubCase(c, "Derive DH key success");

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_operation(c, &session, &op,
			TEE_ALG_DH_DERIVE_SHARED_SECRET, TEE_MODE_DERIVE,
			derive_key_max_keysize)))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_transient_object(c, & session,
			TEE_TYPE_DH_KEYPAIR, derive_key_max_keysize,
			&key_handle)))
		goto out;

	xtest_add_attr(&param_count, params, TEE_ATTR_DH_PRIME,
		       ARRAY(derive_key_dh_prime));

	xtest_add_attr(&param_count, params, TEE_ATTR_DH_BASE,
		       ARRAY(derive_key_dh_base));

	xtest_add_attr(&param_count, params, TEE_ATTR_DH_PUBLIC_VALUE,
		       ARRAY(derive_key_dh_public_value));

	xtest_add_attr(&param_count, params, TEE_ATTR_DH_PRIVATE_VALUE,
		       ARRAY(derive_key_dh_private_value));

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_populate_transient_object(c, &session, key_handle,
			params, param_count)))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_set_operation_key(c, &session, op, key_handle)))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_free_transient_object(c, & session, key_handle)))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_transient_object(c, &session,
			TEE_TYPE_GENERIC_SECRET, derive_key_max_keysize,
			&sv_handle)))
		goto out;

	/* reuse but reset params and param-count */
	param_count = 0;

	xtest_add_attr(&param_count, params, TEE_ATTR_DH_PUBLIC_VALUE,
		       ARRAY(derive_key_dh_public_value_2));

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_derive_key(c, &session, op, sv_handle, params,
			param_count)))
		goto out;

	out_size = sizeof(out);
	memset(out, 0, sizeof(out));
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_get_object_buffer_attribute(c, &session, sv_handle,
			TEE_ATTR_SECRET_VALUE, out, &out_size)))
		goto out;

	if (!ADBG_EXPECT_BUFFER(c, derive_key_dh_shared_secret,
				sizeof(derive_key_dh_shared_secret), out,
				out_size))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_free_operation(c, &session, op)))
		goto out;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_free_transient_object(c, &session, sv_handle)))
		goto out;
out:
	Do_ADBG_EndSubCase(c, "Derive DH key success");
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4008, xtest_tee_test_4008,
		"Test TEE Internal API Derive key");

static void xtest_tee_test_4009(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	TEE_OperationHandle op;
	TEE_ObjectHandle key_handle;
	TEE_ObjectHandle sv_handle;
	TEE_Attribute params[4];
	size_t param_count = 0;
	uint8_t out[2048];
	size_t out_size;
	uint32_t size_bytes;
	uint32_t i;
	struct derive_key_ecdh_t *pt;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	for (i = 0; i < ARRAY_SIZE(derive_key_ecdh); i++) {
		pt = &derive_key_ecdh[i];

		if (pt->level > level)
			continue;

		Do_ADBG_BeginSubCase(c, "Derive ECDH key - algo = 0x%x",
				     pt->algo);
		size_bytes = (pt->keysize + 7) / 8;
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_operation(c, &session, &op,
				pt->algo,
				TEE_MODE_DERIVE, pt->keysize)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_transient_object(c, & session,
				TEE_TYPE_ECDH_KEYPAIR, pt->keysize,
				&key_handle)))
			goto out;

		param_count = 0;
		xtest_add_attr_value(&param_count, params,
				     TEE_ATTR_ECC_CURVE, pt->curve, 0);
		xtest_add_attr(&param_count, params,
			       TEE_ATTR_ECC_PRIVATE_VALUE,
			       pt->private, size_bytes);
		/*
		 * The public value is not used. This is why we provide
		 * another buffer
		 */
		xtest_add_attr(&param_count, params,
				TEE_ATTR_ECC_PUBLIC_VALUE_X,
			       pt->private, size_bytes);
		xtest_add_attr(&param_count, params,
				TEE_ATTR_ECC_PUBLIC_VALUE_Y,
			       pt->private, size_bytes);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_populate_transient_object(c,
					&session,
					key_handle, params, param_count)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_set_operation_key(c, &session, op,
					key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
				ta_crypt_cmd_free_transient_object(c, & session,
					key_handle)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_allocate_transient_object(c, &session,
				TEE_TYPE_GENERIC_SECRET, size_bytes * 8,
				&sv_handle)))
			goto out;

		/* reuse but reset params and param-count */
		param_count = 0;

		xtest_add_attr(&param_count, params,
			       TEE_ATTR_ECC_PUBLIC_VALUE_X,
			       pt->public_x, size_bytes);
		xtest_add_attr(&param_count, params,
			       TEE_ATTR_ECC_PUBLIC_VALUE_Y,
			       pt->public_y, size_bytes);

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_derive_key(c, &session, op, sv_handle,
					        params, param_count)))
			goto out;

		out_size = sizeof(out);
		memset(out, 0, sizeof(out));
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_get_object_buffer_attribute(c, &session,
				sv_handle,
				TEE_ATTR_SECRET_VALUE, out, &out_size)))
			goto out;

		if (!ADBG_EXPECT_BUFFER(c, pt->out, size_bytes,
					out, out_size))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_operation(c, &session, op)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_free_transient_object(c, &session,
							   sv_handle)))
			goto out;

		Do_ADBG_EndSubCase(c, "Derive ECDH key - algo = 0x%x",
				   pt->algo);
	}

	goto noerror;

out:
	Do_ADBG_EndSubCase(c, "Derive ECDH key - algo = 0x%x", pt->algo);

noerror:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4009, xtest_tee_test_4009,
		"Test TEE Internal API Derive key ECDH");

#ifdef CFG_SECURE_KEY_SERVICES
/*
 * The test below belongs to the regression 41xx test. As it relies on test
 * vectors defined for the 40xx test, this test sequence is implemented here.
 * The test below checks compliance of crypto algorithms called through the
 * PKCS#11 interface.
 */
void run_xtest_tee_test_4218(ADBG_Case_t *c, CK_SLOT_ID slot);

void run_xtest_tee_test_4218(ADBG_Case_t *c, CK_SLOT_ID slot)
{
	uint32_t size_bytes;
	uint32_t i;
	struct derive_key_ecdh_t *pt;
	CK_OBJECT_HANDLE derived_key_handle;
	CK_OBJECT_HANDLE priv_key_handle;
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
	CK_RV rv;

	rv = C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
			   NULL, 0, &session);
	if (!ADBG_EXPECT_CK_OK(c, rv))
		goto out;

	for (i = 0; i < ARRAY_SIZE(derive_key_ecdh); i++) {
		CK_ATTRIBUTE derived_key_template[] = {
			{ CKA_EXTRACTABLE, &(CK_BBOOL){CK_TRUE},
						sizeof(CK_BBOOL) },
			{ CKA_KEY_TYPE,	&(CK_KEY_TYPE){CKK_GENERIC_SECRET},
						sizeof(CK_KEY_TYPE) },
			{ CKA_CLASS, &(CK_OBJECT_CLASS){CKO_SECRET_KEY},
						sizeof(CK_OBJECT_CLASS) },
			{ CKA_VALUE_LEN, &(CK_ULONG){0}, sizeof(CK_ULONG) },
		};
		uint8_t derived_key_value[16];
		CK_ATTRIBUTE get_derived_key_template[] = {
			{ CKA_VALUE, &(CK_VOID_PTR){&derived_key_value},
						sizeof(derived_key_value) },
		};
		CK_MECHANISM ck_mechanism = { 0 };
		CK_ECDH1_DERIVE_PARAMS *edch_params;
		CK_ULONG ck_key_bit_size;

		pt = &derive_key_ecdh[i];

		if (pt->level > level)
			continue;

		size_bytes = (pt->keysize + 7) / 8;

		Do_ADBG_BeginSubCase(c, "Derive ECDH key - (%u bits, %u bytes)",
					pt->keysize, size_bytes);

		if (ck_ec_params_attr_from_tee_curve(cktest_ec_key_priv_attr,
					ARRAY_SIZE(cktest_ec_key_priv_attr),
					pt->curve) ||
		    SET_CK_ATTR(cktest_ec_key_priv_attr, CKA_VALUE,
			        pt->private, size_bytes)) {
			Do_ADBG_Log("Invalid test setup");
			ADBG_EXPECT_TRUE(c, false);
			goto out;
		}

		rv = C_CreateObject(session, cktest_ec_key_priv_attr,
				    ARRAY_SIZE(cktest_ec_key_priv_attr),
				    &priv_key_handle);
		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		/* Fill Cryptoki mechanism structure from OP-TEE native vectors */
	        if (tee_alg2ckmt(pt->algo, &ck_mechanism) ||
		    ck_mechanism.ulParameterLen !=
				sizeof(CK_ECDH1_DERIVE_PARAMS)) {
			Do_ADBG_Log("Invalid test setup");
			ADBG_EXPECT_TRUE(c, false);
			goto out;
		}

		edch_params = (CK_ECDH1_DERIVE_PARAMS *)ck_mechanism.pParameter;

		/* Public data can be passed as bignum in derive parameters */
		edch_params->pPublicData = malloc(size_bytes * 2);
		if (!edch_params->pPublicData) {
			ADBG_EXPECT_TRUE(c, false);
			goto out;
		}

		ck_key_bit_size = pt->keysize;
		if (SET_CK_ATTR(derived_key_template, CKA_VALUE_LEN,
			        &ck_key_bit_size, sizeof(CK_ULONG))) {
			Do_ADBG_Log("Invalid test setup");
			ADBG_EXPECT_TRUE(c, false);
			goto out;
		}

		memcpy(&edch_params->pPublicData[0],
				pt->public_x, size_bytes);
		memcpy(&edch_params->pPublicData[size_bytes],
				pt->public_y, size_bytes);
		edch_params->ulPublicDataLen = size_bytes * 2;

		rv = C_DeriveKey(session, &ck_mechanism, priv_key_handle,
				 &derived_key_template[0],
				 ARRAY_SIZE(derived_key_template),
				 &derived_key_handle);

		free(cktest_ecdh_params.pPublicData);
		cktest_ecdh_params.pPublicData = NULL;

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		memset(get_derived_key_template[0].pValue, 0,
			get_derived_key_template[0].ulValueLen);

		rv = C_GetAttributeValue(session, derived_key_handle,
					 &get_derived_key_template[0], 1);
#if 0
		if (!ADBG_EXPECT_CK_OK(c, rv)) {
			goto out;

		if (!ADBG_EXPECT_BUFFER(c, pt->out, size_bytes,
					get_derived_key_template[0].pValue,
					get_derived_key_template[0].ulValueLen))
			goto out;
#endif

		rv = C_DestroyObject(session, priv_key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		rv = C_DestroyObject(session, derived_key_handle);

		if (!ADBG_EXPECT_CK_OK(c, rv))
			goto out;

		Do_ADBG_EndSubCase(c, NULL);
	}

	goto noerror;

out:
	Do_ADBG_EndSubCase(c, NULL);

noerror:
	rv = C_CloseSession(session);
	ADBG_EXPECT_CK_OK(c, rv);
}
#endif /*CFG_SECURE_KEY_SERVICES*/

static void xtest_tee_test_4010(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	TEE_ObjectHandle o;
	static const uint8_t large_key[1024] = { 1, 2, 3, 4, 5, 6 };
	static const TEE_Attribute attr = {
		.attributeID = TEE_ATTR_SECRET_VALUE,
		.content.ref.buffer = (void *)large_key,
		.content.ref.length = sizeof(large_key),
	};

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_allocate_transient_object(c, &session,
			TEE_TYPE_HMAC_SHA256, 1024, &o)))
		goto out;

	ADBG_EXPECT_TEEC_RESULT(c, TEEC_ERROR_TARGET_DEAD,
		ta_crypt_cmd_populate_transient_object(c, &session, o,
						       &attr, 1));

out:
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4010, xtest_tee_test_4010,
		"Test TEE Internal API create transient object (negative)");

static void xtest_tee_test_4011(ADBG_Case_t *c)
{
	TEEC_Session s = { 0 };
	size_t key_size = 512;
	TEE_ObjectHandle key;
	TEE_OperationHandle ops;
	TEE_OperationHandle opv;
	TEE_OperationHandle ope;
	TEE_OperationHandle opd;
	uint32_t ret_orig;
	uint8_t in[TEE_SHA1_HASH_SIZE];
	uint8_t out[1024];
	uint8_t tmp[1024];
	size_t out_size;
	size_t tmp_size;
	size_t n;
	size_t m;
	size_t i = 0;

	/* Setup session, initialize message to sign, create a keypair */
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, xtest_teec_open_session(&s,
			&crypt_user_ta_uuid, NULL, &ret_orig)))
		return;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_random_number_generate(c,
			&s, in, sizeof(in))))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_allocate_transient_object(
			c, &s, TEE_TYPE_RSA_KEYPAIR, key_size, &key)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_generate_key(c, &s,
			key, key_size, NULL, 0)))
		goto out;

	/* Allocate operations for sign, verify, encrypt and decrypt */
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_allocate_operation(c, &s,
			&ops, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_SIGN,
			key_size)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_allocate_operation(c, &s,
			&opv, TEE_ALG_RSASSA_PKCS1_V1_5_SHA1, TEE_MODE_VERIFY,
			key_size)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_allocate_operation(c, &s,
			&ope, TEE_ALG_RSA_NOPAD, TEE_MODE_ENCRYPT, key_size)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c, ta_crypt_cmd_allocate_operation(c, &s,
			&opd, TEE_ALG_RSA_NOPAD, TEE_MODE_DECRYPT, key_size)))
		goto out;

	/* Assign the keypair to all operations */
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_set_operation_key(c, &s, ops, key)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_set_operation_key(c, &s, opv, key)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_set_operation_key(c, &s, ope, key)))
		goto out;
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		ta_crypt_cmd_set_operation_key(c, &s, opd, key)))
		goto out;

	/*
	 * The core of the test case is inspired by the one in libtomcrypt:
	 * https://github.com/libtom/libtomcrypt/blob/6ad52252688bb34f90b5e79da4830a927e87b81f/testprof/rsa_test.c#L398
	 *
	 * Testcase for Bleichenbacher attack
	 *
	 * (1) Create a valid signature
	 * (2) Check that it can be verified
	 * (3) Transform the package to fetch plain text (using the encrypt
	 *     operation in GP TEE Internal API)
	 * (4) Forge the structure of PKCS#1-EMSA encoded data
	 * (4.1) Search for start and end of the padding string
	 * (4.2) Move the signature to the front of the padding string
	 * (4.3) Zero the message until the end
	 * (5) Transform the package back (using the decrypt operation in
	 *     GP TEE Internal API)
	 * (6) The result should not be valid if the implementation is robust.
	 */


	for (i = 0; i < 9; i++) {
		Do_ADBG_Log("Iteration %zu", i);

		/* 1 */
		out_size = sizeof(out);
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_asymmetric_sign(c, &s, ops, NULL, 0,
				in, sizeof(in), out, &out_size)))
			goto out;

		/* 2 */
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_asymmetric_verify(c, &s, opv, NULL, 0,
				in, sizeof(in), out, out_size)))
			goto out;

		/* 3 */
		tmp_size = sizeof(tmp);
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_asymmetric_encrypt(c, &s, ope, NULL, 0,
				out, out_size, tmp, &tmp_size)))
			goto out;

		if (!ADBG_EXPECT_COMPARE_UNSIGNED(c, tmp_size, <=, sizeof(tmp)))
			goto out;

		/* 4.1 */
		for (n = 0; n < tmp_size - i; n++)
			if (tmp[n] == 0xff)
				break;

		/* Shall find at least a padding start before buffer end */
	        if (!ADBG_EXPECT_COMPARE_UNSIGNED(c, n, <, tmp_size - i - 1))
			goto out;

		for (m = n + 1; m < tmp_size; m++)
			if (tmp[m] != 0xff)
				break;

		/* 4.2 */
		memmove(tmp + n + i, tmp + m, tmp_size - m);

		/* 4.3 */
		n = n + i + tmp_size - m;
		memset(tmp + n, 0, tmp_size - n);

		/* 5 */
		out_size = sizeof(out);
		if (!ADBG_EXPECT_TEEC_SUCCESS(c,
			ta_crypt_cmd_asymmetric_decrypt(c, &s, opd, NULL, 0,
				tmp, tmp_size, out, &out_size)))
			goto out;

		/* 6 */
		if (!ADBG_EXPECT_TEEC_RESULT(c, TEE_ERROR_SIGNATURE_INVALID,
			ta_crypt_cmd_asymmetric_verify(c, &s, opv, NULL, 0,
				in, sizeof(in), out, out_size)))
			goto out;
	}

out:
	TEEC_CloseSession(&s);
}
ADBG_CASE_DEFINE(regression, 4011, xtest_tee_test_4011,
		"Test TEE Internal API Bleichenbacher attack (negative)");

#ifdef CFG_SYSTEM_PTA
static void xtest_tee_test_4012(ADBG_Case_t *c)
{
	TEEC_Session session = { 0 };
	uint32_t ret_orig;
	TEEC_Operation op = TEEC_OPERATION_INITIALIZER;
	/* Fortuna PRNG requires seed <= 32 bytes */
	uint8_t pool_input[32] = {};
	time_t t;
	struct tm tm_local;

	t = time(NULL);
	tm_local = *localtime(&t);

	memcpy((void *)pool_input, (void *)&tm_local,
	       sizeof(pool_input) < sizeof(tm_local) ?
	       sizeof(pool_input) : sizeof(tm_local));


	op.params[0].tmpref.buffer = pool_input;
	op.params[0].tmpref.size = sizeof(pool_input);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	if (!ADBG_EXPECT_TEEC_SUCCESS(c,
		xtest_teec_open_session(&session, &crypt_user_ta_uuid, NULL,
					&ret_orig)))
		return;

	(void)ADBG_EXPECT_TEEC_SUCCESS(c,
				       TEEC_InvokeCommand(&session,
					TA_CRYPT_CMD_SEED_RNG_POOL,
					&op,
					&ret_orig));
	TEEC_CloseSession(&session);
}
ADBG_CASE_DEFINE(regression, 4012, xtest_tee_test_4012,
		"Test seeding RNG entropy");
#endif /*CFG_SYSTEM_PTA*/
