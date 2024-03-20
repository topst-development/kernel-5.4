/* linux/drivers/char/tcc_hsm_cmd.h
 *
 * Copyright (C) 2009, 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef TCC_HSM_CMD_H
#define TCC_HSM_CMD_H

// clang-format off

/* Error Code 0x000000XX */
#define TCCHSM_SUCCESS 					(0x00000000u)
#define TCCHSM_ERR 						(0x00000001u)
#define TCCHSM_ERR_INVALID_PARAM 		(0x00000002u)
#define TCCHSM_ERR_INVALID_STATE 		(0x00000003u)
#define TCCHSM_ERR_INVALID_MEMORY		(0x00000004u)
#define TCCHSM_ERR_UNSUPPORTED_FUNC 	(0x00000005u)
#define TCCHSM_ERR_OTP 					(0x00000006u)
#define TCCHSM_ERR_CRYPTO 				(0x00000007u)
#define TCCHSM_ERR_OCCUPIED_RESOURCE 	(0x00000008u)
#define TCCHSM_ERR_IMG_INTEGRITY 		(0x00000009u)
#define TCCHSM_ERR_RBID_MISMATCH 		(0x0000000Au)
#define TCCHSM_ERR_IMGID_MISMATCH 		(0x0000000Bu)
#define TCCHSM_ERR_ATTR_MISMATCH 		(0x0000000Cu)
#define TCCHSM_ERR_VERSION_MISMATCH		(0x0000000Du)
/* Error Code 0x0000XX00 */
#define ERROR_AES						(0x00001000u)
#define ERROR_AES_ECB					(0x00001100u)
#define ERROR_AES_CBC					(0x00001200u)
#define ERROR_CMAC_AES					(0x00001800u)
#define ERROR_SHA						(0x00002000u)
#define ERROR_SHA1						(0x00002100u)
#define ERROR_SHA2						(0x00002200u)
#define ERROR_SHA3						(0x00002300u)
#define ERROR_SHA256					(0x00002800u)
#define ERROR_RNG						(0x00004000u)
#define ERROR_RNG_TRNG					(0x00004100u)
#define ERROR_RNG_PRNG					(0x00004200u)
#define ERROR_RNG_PRNG_Instantiate		(0x00004300u)
#define ERROR_RNG_PRNG_Reseed			(0x00004400u)
#define ERROR_RNG_PRNG_Generate			(0x00004500u)
#define ERROR_RSA						(0x00005000u)
#define ERROR_RSA_PSS_VRFY				(0x00005240u)
#define ERROR_RSA_PSS_VRFY_DIGEST		(0x00005260u)
#define ERROR_RSA_GEN_KEY				(0x00005300u)
#define ERROR_ECC						(0x00006000u)
#define ERROR_ECDSA						(0x00007000u)
#define ERROR_ECDSA_NIST_VRFY			(0x00007110u)
#define ERROR_ECDSA_BP_VRFY				(0x00007210u)
#define ERROR_PKA						(0x0000E000u)
#define ERROR_FAIL						(0x0000F100u)
/* Error Code 0x00XX0000 */
#define INVALID_LEN						(0x00010000u)
#define INVALID_SEL						(0x00FE0000u)
#define INVALID_VAL						(0x00FF0000u)
#define INVALID_STS						(0x00FD0000u)
#define INVALID_SZ						(0x00FC0000u)
#define INVALID_FORM					(0x00FB0000u)
#define INVALID_STATUS					(0x00190000u)
/* Error Code 0xXX000000 */
#define ERR_IV							(0x02000000u)
#define ERR_TAG							(0x03000000u)
#define RR_KEY							(0x04000000u)
#define ERR_BLOCK						(0x05000000u)
#define ERR_MSG							(0x06000000u)
#define ERR_MODE						(0x07000000u)
#define ERR_OID_ALG						(0x08000000u)
#define ERR_OID_SIZE					(0x09000000u)
#define ERR_SIGNATURE					(0x0A000000u)
#define ERR_PUBLICKEY					(0x0B000000u)
#define ERR_BUSY						(0x10000000u)
#define ERR_KAT							(0x14000000u)
#define ERR_HT							(0x15000000u)
#define ERR_RANDOM						(0x16000000u)
#define ERR_SALT						(0x17000000u)
#define ERR_STATE						(0x22000000u)
#define ERR_ENTROPY						(0x23000000u)
#define ERR_RESEED_COUNTER				(0x26000000u)
#define ERR_INPUT_STRING				(0x27000000u)
#define ERR_REQ_RNG						(0x28000000u)
#define ERR_SEED						(0x29000000u)
#define ERR_LABEL						(0x30000000u)
#define ERR_HW							(0xFF000000u)
#define ERR_DECRYPTION					(0xDD000000u)


/* HSM Command */
#define REQ_HSM_RUN_AES                         (0x10010000U)
#define REQ_HSM_RUN_AES_BY_KT                   (0x10020000U)
#define REQ_HSM_RUN_SM4                         (0x10030000U)
#define REQ_HSM_RUN_SM4_BY_KT                   (0x10040000U)

#define REQ_HSM_VERIFY_CMAC                     (0x10110000U)
#define REQ_HSM_VERIFY_CMAC_BY_KT               (0x10120000U)
#define REQ_HSM_GEN_GMAC                        (0x10130000U)
#define REQ_HSM_GEN_GMAC_BY_KT                  (0x10140000U)
#define REQ_HSM_GEN_HMAC                        (0x10150000U)
#define REQ_HSM_GEN_HMAC_BY_KT                  (0x10160000U)
#define REQ_HSM_GEN_SM3_HMAC                    (0x10170000U)
#define REQ_HSM_GEN_SM3_HMAC_BY_KT              (0x10180000U)

#define REQ_HSM_GEN_SHA                         (0x10210000U)
#define REQ_HSM_GEN_SM3                         (0x10220000U)

#define REQ_HSM_RUN_ECDSA_SIGN                  (0x10310000U)
#define REQ_HSM_RUN_ECDSA_VERIFY                (0x10320000U)
#define REQ_HSM_RUN_ECDH_PUBKEY_COMPUTE		    (0x10330000u)
#define REQ_HSM_RUN_ECDH_PHASE_I			    (0x10340000u)
#define REQ_HSM_RUN_ECDH_PHASE_II			    (0x10350000u)
#define REQ_HSM_RUN_ECDSA_SIGN_BY_KT		    (0x10360000u)
#define REQ_HSM_RUN_ECDSA_VERIFY_BY_KT		    (0x10370000u)

#define REQ_HSM_RUN_RSASSA_PKCS_SIGN            (0x10550000U)
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY          (0x10560000U)
#define REQ_HSM_RUN_RSASSA_PSS_SIGN             (0x10570000U)
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY           (0x10580000U)
#define REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT      (0x105D0000U)
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT    (0x105E0000U)
#define REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT       (0x105F0000U)
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT     (0x10600000U)

#define REQ_HSM_GET_RNG                         (0x10610000U)

#define REQ_HSM_WRITE_OTP                       (0x10710000U)
#define REQ_HSM_WRITE_SNOR                      (0x10720000U)

#define REQ_HSM_SET_KEY_FROM_OTP                (0x10810000U)
#define REQ_HSM_SET_KEY_FROM_SNOR               (0x10820000U)
#define REQ_HSM_SET_MODN_FROM_OTP               (0x10840000u)
#define REQ_HSM_SET_MODN_FROM_SNOR              (0x10850000u)


#define REQ_HSM_GET_VER                         (0x20010000U)

#define HSM_NONE_DMA                            (0U)
#define HSM_DMA                                 (1U)

#define TCC_HSM_AES_KEY_SIZE        (32U)
#define TCC_HSM_AES_IV_SIZE         (32U)
#define TCC_HSM_AES_TAG_SIZE        (32U)
#define TCC_HSM_AES_AAD_SIZE        (32U)

#define TCC_HSM_MAC_KEY_SIZE        (32U)
#define TCC_HSM_MAC_MSG_SIZE        (32U)

#define TCC_HSM_HASH_DIGEST_SIZE	(64U)
#define TCC_HSM_ECDSA_KEY_SIZE		(64U)
#define TCC_HSM_ECDSA_P521_KEY_SIZE	(68U)
#define TCC_HSM_ECDSA_DIGEST_SIZE	(64U)
#define TCC_HSM_ECDSA_SIGN_SIZE		(64U)

#define TCC_HSM_RSA_MODN_SIZE		(512U)
#define TCC_HSM_RSA_DIG_SIZE		(64U)
#define TCC_HSM_RSA_SIG_SIZE		(512U)
// clang-format on

struct tcc_hsm_ioctl_set_key_param {
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_set_modn_param {
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_aes_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_AES_KEY_SIZE];
	uint32_t key_size;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint32_t counter_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_aes_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint32_t counter_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_MAC_KEY_SIZE];
	uint32_t key_size;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_hash_param {
	uint32_t obj_id;
	ulong src;
	uint32_t src_size;
	uint8_t digest[TCC_HSM_HASH_DIGEST_SIZE];
	uint32_t digest_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_ecdsa_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_ECDSA_KEY_SIZE];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_ecdsa_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsassa_param {
	uint32_t obj_id;
	uint8_t modN[TCC_HSM_RSA_MODN_SIZE];
	uint32_t modN_size;
	uint8_t key[TCC_HSM_RSA_MODN_SIZE];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsassa_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_write_param {
	uint32_t addr;
	ulong data;
	uint32_t data_size;
};

struct tcc_hsm_ioctl_rng_param {
	ulong rng;
	uint32_t rng_size;
};

struct tcc_hsm_ioctl_version_param {
	uint32_t x;
	uint32_t y;
	uint32_t z;
};

struct tcc_hsm_ioctl_ecdh_key_param {
	uint32_t key_type;
	uint32_t obj_id;
	uint8_t prikey[TCC_HSM_ECDSA_P521_KEY_SIZE];
	uint32_t prikey_size;
	uint8_t pubkey[TCC_HSM_ECDSA_P521_KEY_SIZE * 2];
	uint32_t pubkey_size;
};

uint32_t tcc_hsm_cmd_set_key(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_set_key_param *param);
uint32_t tcc_hsm_cmd_set_modn(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_set_modn_param *param);
uint32_t tcc_hsm_cmd_run_aes(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_aes_param *param, uint32_t desc);
uint32_t tcc_hsm_cmd_run_aes_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_aes_by_kt_param *param, uint32_t desc);
uint32_t tcc_hsm_cmd_gen_mac(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_mac_param *param);
uint32_t tcc_hsm_cmd_gen_mac_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_mac_by_kt_param *param);
uint32_t tcc_hsm_cmd_gen_hash(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_hash_param *param);
uint32_t tcc_hsm_cmd_run_ecdh_phaseI(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdh_key_param *param);
uint32_t tcc_hsm_cmd_run_ecdsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdsa_param *aram);
uint32_t tcc_hsm_cmd_run_ecdsa_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdsa_by_kt_param *aram);
uint32_t tcc_hsm_cmd_run_rsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_rsassa_param *param,
	uint32_t modN, uint32_t key, uint32_t digest, uint32_t sig);
uint32_t tcc_hsm_cmd_run_rsa_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_rsassa_by_kt_param *param,
	uint32_t digest, uint32_t sig);
uint32_t tcc_hsm_cmd_write(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_write_param *param);
uint32_t tcc_hsm_cmd_get_version(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_version_param *param);
uint32_t tcc_hsm_cmd_get_rand(
	uint32_t device_id, uint32_t req, uint32_t rng, uint32_t rng_size);
#endif /* TCC_HSM_CMD_H */
