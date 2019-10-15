/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if 1
#define SYS_LOG_DOMAIN "15.4 crypto"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1
#endif

#include <zephyr.h>
#include <errno.h>

#include <net/net_core.h>

#include <crypto/cipher.h>
#include <crypto/cipher_structs.h>

#ifdef CONFIG_IEEE802154_CC2520_CRYPTO_DRV_NAME
#define IEEE802154_CRYPTO_DRV_NAME	CONFIG_IEEE802154_CC2520_CRYPTO_DRV_NAME
#endif

struct cipher_ctx enc;
struct cipher_ctx dec;
struct cipher_pkt pkt;
struct cipher_aead_pkt apkt;
u8_t buf[128];

static void print_caps(struct device *dev)
{
	int caps = cipher_query_hwcaps(dev);

	printk("Crpyto hardware capabilities:\n");

	if (caps & CAP_RAW_KEY) {
		printk("\tCAPS_RAW_KEY\n");
	}

	if (caps & CAP_INPLACE_OPS) {
		printk("\tCAP_INPLACE_OPS\n");
	}

	if (caps & CAP_SYNC_OPS) {
		printk("\tCAP_SYNC_OPS\n");
	}
}

static void print_buffer(u8_t *buf, u8_t len)
{
	int i;

	printk("Buffer content:\n");

	printk("\t");

	for (i = 0; len > 0; len--, i++) {
		printk("%02x ", *buf++);

		if (i == 7) {
			printk("\n\t");
			i = -1;
		}
	}

	printk("\n");
}

static bool verify_result(u8_t *result, int result_len,
			  u8_t *verify, int verify_len)
{
	if (result_len != verify_len) {
		NET_ERR("Result and verification length don't match (%i vs %i)",
			result_len, verify_len);
		return false;
	}

	NET_INFO("Verification data:");
	print_buffer(verify, verify_len);

	NET_INFO("Result data:");
	print_buffer(result, result_len);

	if (memcmp(result, verify, result_len)) {
		return false;
	}

	return true;
}

static void ds_test(struct device *dev)
{
	u8_t key[] = { 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
			  0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf };

	u8_t auth_nonce[] = { 0xac, 0xde, 0x48, 0x00, 0x00, 0x00, 0x00,
				 0x01, 0x00, 0x00, 0x00, 0x05, 0x02 };

	u8_t auth_data[] = { 0x08, 0xd0, 0x84, 0x21, 0x43, 0x01, 0x00,
				0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0x02,
				0x05, 0x00, 0x00, 0x00, 0x55, 0xcf, 0x00,
				0x00, 0x51, 0x52, 0x53, 0x54 };
	u8_t auth_result[] = { 0x08, 0xd0, 0x84, 0x21, 0x43, 0x01, 0x00,
				  0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0x02,
				  0x05, 0x00, 0x00, 0x00, 0x55, 0xcf, 0x00,
				  0x00, 0x51, 0x52, 0x53, 0x54, 0xca, 0x45,
				  0x91, 0x8d, 0x3d, 0x82, 0xe5, 0xd0};
	u8_t enc_dec_nonce[] = { 0xac, 0xde, 0x48, 0x00, 0x00, 0x00, 0x00,
				    0x01, 0x00, 0x00, 0x00, 0x05, 0x04 };
	u8_t enc_dec_data[] = { 0x69, 0xdc, 0x84, 0x21, 0x43, 0x02, 0x00,
				   0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0x01,
				   0x00, 0x00, 0x00, 0x00, 0x48, 0xde, 0xac,
				   0x04, 0x05, 0x00, 0x00, 0x00, 0x61, 0x62,
				   0x63, 0x64 };
	u8_t enc_dec_result[] = { 0x69, 0xdc, 0x84, 0x21, 0x43, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0x01,
				     0x00, 0x00, 0x00, 0x00, 0x48, 0xde, 0xac,
				     0x04, 0x05, 0x00, 0x00, 0x00, 0x7c, 0x64,
				     0xc5, 0x0a };
	u8_t both_op_nonce[] = { 0xac, 0xde, 0x48, 0x00, 0x00, 0x00, 0x00,
				    0x01, 0x00, 0x00, 0x00, 0x05, 0x06 };
	u8_t both_op_data[] = { 0x2b, 0xdc, 0x84, 0x21, 0x43, 0x02, 0x00,
				   0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0xff,
				   0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x48,
				   0xde, 0xac, 0x06, 0x05, 0x00, 0x00, 0x00,
				   0x01, 0xce };
	u8_t both_op_result[] = { 0x2b, 0xdc, 0x84, 0x21, 0x43, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x48, 0xde, 0xac, 0xff,
				     0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x48,
				     0xde, 0xac, 0x06, 0x05, 0x00, 0x00, 0x00,
				     0x01, 0x2a, 0xaa, 0x80, 0xf2, 0x90, 0xb5,
				     0xa3, 0xb6, 0xfe };
	int ret;

	/* Install the key */
	enc.key.bit_stream = key;
	enc.keylen = sizeof(key);
	dec.key.bit_stream = key;
	dec.keylen = sizeof(key);

	/* Setup CCM parameters */
	enc.mode_params.ccm_info.nonce_len = 13;
	dec.mode_params.ccm_info.nonce_len = 13;

	enc.flags = CAP_RAW_KEY | CAP_INPLACE_OPS | CAP_SYNC_OPS;
	dec.flags = CAP_RAW_KEY | CAP_INPLACE_OPS | CAP_SYNC_OPS;

	apkt.pkt = &pkt;

	ret = cipher_begin_session(dev, &enc,
				   CRYPTO_CIPHER_ALGO_AES,
				   CRYPTO_CIPHER_MODE_CCM,
				   CRYPTO_CIPHER_OP_ENCRYPT);
	if (ret) {
		NET_ERR("Cannot start encryption session");
		return;
	}

	ret = cipher_begin_session(dev, &dec,
				   CRYPTO_CIPHER_ALGO_AES,
				   CRYPTO_CIPHER_MODE_CCM,
				   CRYPTO_CIPHER_OP_DECRYPT);
	if (ret) {
		NET_ERR("Cannot start decryption session");
		goto out;
	}

	/* auth_data test: Authentify the packet only */
	memcpy(buf, auth_data, sizeof(auth_data));
	pkt.in_buf = NULL;
	pkt.in_len = 0;
	pkt.out_buf = buf;
	pkt.out_buf_max = sizeof(buf);

	apkt.ad = buf;
	apkt.ad_len = sizeof(auth_data);

	enc.mode_params.ccm_info.tag_len = 8;
	pkt.ctx = &enc;

	ret = cipher_ccm_op(&enc, &apkt, auth_nonce);
	if (ret) {
		NET_ERR("Cannot set authentication!");
		goto enc;
	}

	if (!verify_result(buf, pkt.out_len,
			   auth_result, sizeof(auth_result))) {
		NET_ERR("Authentication setting ds test failed!");
		goto enc;
	}

	dec.mode_params.ccm_info.tag_len = 8;
	pkt.ctx = &dec;

	ret = cipher_ccm_op(&dec, &apkt, auth_nonce);
	if (ret) {
		NET_ERR("Cannot authentify!");
		goto enc;
	}

	NET_INFO("Authentication only test: PASSED");

enc:
	/* enc_dec_data test: Encrypt the packet only */
	memcpy(buf, enc_dec_data, sizeof(enc_dec_data));
	pkt.in_buf = buf + 26;
	pkt.in_len = 4;
	pkt.out_buf = buf;
	pkt.out_buf_max = sizeof(buf);

	apkt.ad = buf;
	apkt.ad_len = sizeof(enc_dec_data) - 4;
	apkt.tag = NULL;

	/* No tag = no MIC, thus no auth */
	enc.mode_params.ccm_info.tag_len = 0;
	pkt.ctx = &enc;

	ret = cipher_ccm_op(&enc, &apkt, enc_dec_nonce);
	if (ret) {
		NET_ERR("Cannot encrypt only!");
		goto out;
	}

	if (!verify_result(buf, pkt.out_len,
			   enc_dec_result, sizeof(enc_dec_result))) {
		NET_ERR("Encryption only ds test failed!");
		goto both;
	}

	dec.mode_params.ccm_info.tag_len = 0;
	pkt.ctx = &dec;

	ret = cipher_ccm_op(&dec, &apkt, enc_dec_nonce);
	if (ret) {
		NET_ERR("Cannot decrypt only!");
		goto both;
	}

	if (!verify_result(buf, pkt.out_len,
			   enc_dec_data, sizeof(enc_dec_data))) {
		NET_ERR("Decryption only ds test failed!");
	}

	NET_INFO("Encryption only test: PASSED");

both:
	/* both_op_data test: Both auth+encryption */
	memcpy(buf, both_op_data, sizeof(both_op_data));
	pkt.in_buf = buf + 29;
	pkt.in_len = 1;
	pkt.out_buf = buf;
	pkt.out_buf_max = sizeof(buf);

	apkt.ad = buf;
	apkt.ad_len = sizeof(both_op_data) - 1;
	apkt.tag = NULL;

	enc.mode_params.ccm_info.tag_len = 8;
	pkt.ctx = &enc;

	ret = cipher_ccm_op(&enc, &apkt, both_op_nonce);
	if (ret) {
		NET_ERR("Cannot do both!");
		goto out;
	}

	if (!verify_result(buf, pkt.out_len,
			   both_op_result, sizeof(both_op_result))) {
		NET_ERR("Both op test failed!");
		goto out;
	}

	pkt.in_len = 1 + 8;
	dec.mode_params.ccm_info.tag_len = 8;
	pkt.ctx = &dec;

	ret = cipher_ccm_op(&dec, &apkt, both_op_nonce);
	if (ret) {
		NET_ERR("Cannot do both!");
		goto out;
	}

	if (!verify_result(buf, pkt.out_len - dec.mode_params.ccm_info.tag_len,
			   both_op_data, sizeof(both_op_data))) {
		NET_ERR("Both op test failed!");
	}

	NET_INFO("Authentication and encryption test: PASSED");
out:
	cipher_free_session(dev, &enc);
	cipher_free_session(dev, &dec);
}

void main(void)
{
	struct device *dev;

	dev = device_get_binding(IEEE802154_CRYPTO_DRV_NAME);
	if (!dev) {
		NET_ERR("Cannot get crypto device binding\n");
		return;
	}

	print_caps(dev);

	ds_test(dev);
}
