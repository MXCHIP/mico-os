#ifndef _CRYPTO_REDEFINE_H_

#define _CRYPTO_REDEFINE_H_

#define InitSha   wc_InitSha
#define ShaUpdate wc_ShaUpdate
#define ShaFinal  wc_ShaFinal
#define ShaHash   wc_ShaHash

#define InitSha256   wc_InitSha256
#define Sha256Update wc_Sha256Update
#define Sha256Final  wc_Sha256Final
#define Sha256Hash   wc_Sha256Hash

#define InitRng           wc_InitRng
#define RNG_GenerateBlock wc_RNG_GenerateBlock
#define RNG_GenerateByte  wc_RNG_GenerateByte
#define FreeRng        wc_FreeRng

#define RsaPrivateKeyDecode   wc_RsaPrivateKeyDecode
#define RsaPublicKeyDecode    wc_RsaPublicKeyDecode
#define RsaPublicKeyDecodeRaw wc_RsaPublicKeyDecodeRaw

#define InitRsaKey       wc_InitRsaKey
#define FreeRsaKey       wc_FreeRsaKey
#define RsaPublicEncrypt wc_RsaPublicEncrypt
#define RsaPrivateDecryptInline wc_RsaPrivateDecryptInline
#define RsaPrivateDecrypt       wc_RsaPrivateDecrypt
#define RsaSSL_Sign             wc_RsaSSL_Sign
#define RsaSSL_VerifyInline     wc_RsaSSL_VerifyInline
#define RsaSSL_Verify           wc_RsaSSL_Verify
#define RsaEncryptSize          wc_RsaEncryptSize
#define RsaFlattenPublicKey     wc_RsaFlattenPublicKey

#define HmacSetKey wc_HmacSetKey
#define HmacUpdate wc_HmacUpdate
#define HmacFinal  wc_HmacFinal

#endif
