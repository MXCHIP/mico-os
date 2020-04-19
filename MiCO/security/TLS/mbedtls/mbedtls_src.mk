#
#  UNPUBLISHED PROPRIETARY mbedtls CODE
#  Copyright (c) 2020 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

GLOBAL_DEFINES      += MBEDTLS_CONFIG_FILE=\"mbedtls_config.h\"

GLOBAL_INCLUDES     += 	mbedtls-2.16.5/include/ \
						mbedtls-2.16.5/mxchip/include

$(NAME)_SOURCES :=	mbedtls-2.16.5/library/aes.c			\
    mbedtls-2.16.5/library/entropy_poll.c		\
    mbedtls-2.16.5/library/ripemd160.c \
    mbedtls-2.16.5/library/aesni.c			\
    mbedtls-2.16.5/library/error.c			\
    mbedtls-2.16.5/library/rsa.c \
    mbedtls-2.16.5/library/arc4.c			\
    mbedtls-2.16.5/library/gcm.c			\
    mbedtls-2.16.5/library/rsa_internal.c \
    mbedtls-2.16.5/library/aria.c			\
    mbedtls-2.16.5/library/havege.c			\
    mbedtls-2.16.5/library/sha1.c \
    mbedtls-2.16.5/library/asn1parse.c		\
    mbedtls-2.16.5/library/hkdf.c			\
    mbedtls-2.16.5/library/sha256.c \
    mbedtls-2.16.5/library/asn1write.c		\
    mbedtls-2.16.5/library/hmac_drbg.c		\
    mbedtls-2.16.5/library/sha512.c \
    mbedtls-2.16.5/library/base64.c			\
    mbedtls-2.16.5/library/md.c			\
    mbedtls-2.16.5/library/ssl_cache.c \
    mbedtls-2.16.5/library/bignum.c			\
    mbedtls-2.16.5/library/md2.c			\
    mbedtls-2.16.5/library/ssl_ciphersuites.c \
    mbedtls-2.16.5/library/blowfish.c		\
    mbedtls-2.16.5/library/md4.c			\
    mbedtls-2.16.5/library/ssl_cli.c \
    mbedtls-2.16.5/library/camellia.c		\
    mbedtls-2.16.5/library/md5.c			\
    mbedtls-2.16.5/library/ssl_cookie.c \
    mbedtls-2.16.5/library/ccm.c			\
    mbedtls-2.16.5/library/md_wrap.c		\
    mbedtls-2.16.5/library/ssl_srv.c \
    mbedtls-2.16.5/library/certs.c			\
    mbedtls-2.16.5/library/memory_buffer_alloc.c	\
    mbedtls-2.16.5/library/ssl_ticket.c \
    mbedtls-2.16.5/library/chacha20.c		\
    mbedtls-2.16.5/library/net_sockets.c		\
    mbedtls-2.16.5/library/ssl_tls.c \
    mbedtls-2.16.5/library/chachapoly.c		\
    mbedtls-2.16.5/library/nist_kw.c		\
    mbedtls-2.16.5/library/threading.c \
    mbedtls-2.16.5/library/cipher.c			\
    mbedtls-2.16.5/library/oid.c			\
    mbedtls-2.16.5/library/timing.c \
    mbedtls-2.16.5/library/cipher_wrap.c		\
    mbedtls-2.16.5/library/padlock.c		\
    mbedtls-2.16.5/library/version.c \
    mbedtls-2.16.5/library/cmac.c			\
    mbedtls-2.16.5/library/pem.c			\
    mbedtls-2.16.5/library/version_features.c \
    mbedtls-2.16.5/library/ctr_drbg.c		\
    mbedtls-2.16.5/library/pk.c			\
    mbedtls-2.16.5/library/x509.c \
    mbedtls-2.16.5/library/debug.c			\
    mbedtls-2.16.5/library/pk_wrap.c		\
    mbedtls-2.16.5/library/x509_create.c \
    mbedtls-2.16.5/library/des.c			\
    mbedtls-2.16.5/library/pkcs11.c			\
    mbedtls-2.16.5/library/x509_crl.c \
    mbedtls-2.16.5/library/dhm.c			\
    mbedtls-2.16.5/library/pkcs12.c			\
    mbedtls-2.16.5/library/x509_crt.c \
    mbedtls-2.16.5/library/ecdh.c			\
    mbedtls-2.16.5/library/pkcs5.c			\
    mbedtls-2.16.5/library/x509_csr.c \
    mbedtls-2.16.5/library/ecdsa.c			\
    mbedtls-2.16.5/library/pkparse.c		\
    mbedtls-2.16.5/library/x509write_crt.c \
    mbedtls-2.16.5/library/ecjpake.c		\
    mbedtls-2.16.5/library/pkwrite.c		\
    mbedtls-2.16.5/library/x509write_csr.c \
    mbedtls-2.16.5/library/ecp.c			\
    mbedtls-2.16.5/library/platform.c		\
    mbedtls-2.16.5/library/xtea.c \
    mbedtls-2.16.5/library/ecp_curves.c		\
    mbedtls-2.16.5/library/platform_util.c \
    mbedtls-2.16.5/library/entropy.c		\
    mbedtls-2.16.5/library/poly1305.c 
					
$(NAME)_SOURCES +=	mbedtls-2.16.5/mxchip/library/platform.c \
                    mbedtls-2.16.5/mxchip/library/threading_alt.c \
                    mbedtls-2.16.5/mxchip/library/net_sockets.c\
                    mbedtls-2.16.5/mxchip/library/ssl_wrap.c \
                    mbedtls-2.16.5/mxchip/library/crypto_wrap.c \
                    mbedtls-2.16.5/mxchip/library/hc128.c

					

