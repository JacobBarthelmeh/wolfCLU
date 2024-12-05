/* clu_verify.c
 *
 * Copyright (C) 2006-2021 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#include <wolfclu/clu_header_main.h>
#include <wolfclu/clu_log.h>
#include <wolfclu/sign-verify/clu_verify.h>
#include <wolfclu/x509/clu_cert.h>
#include <wolfclu/sign-verify/clu_sign.h> /* for RSA_SIG_VER, ECC_SIG_VER,
                                           * and ED25519_SIG_VER */
#ifndef WOLFCLU_NO_FILESYSTEM

# if 0
static int wolfCLU_generate_public_key_ed25519(char* privKey, int inForm, byte* outBuf,
                                               word32 outLen)
{
#ifdef HAVE_ED25519
    int ret;
    int privFileSz;
    word32 index = 0;

    XFILE privKeyFile = NULL;
    byte* keyBuf = NULL;
    ed25519_key key;

    XMEMSET(&key, 0, sizeof(key));

    /* initialize ED25519 key */
    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wolfCLU_LogError("Failed to initialize ed25519 key\nRET: %d", ret);
    }

    /* open, read, and store ED25519 key */
    if (ret == 0) {
        privKeyFile = XFOPEN(privKey, "rb");
        if (privKeyFile == NULL) {
            wolfCLU_LogError("unable to open file %s", privKey);
            ret = BAD_FUNC_ARG;
        }
    }
    if (ret == 0) {
        XFSEEK(privKeyFile, 0, SEEK_END);
        privFileSz = (int)XFTELL(privKeyFile);
        keyBuf = (byte*)XMALLOC(privFileSz+1, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (keyBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(keyBuf, 0, privFileSz+1);
        if (XFSEEK(privKeyFile, 0, SEEK_SET) != 0 ||
            (int)XFREAD(keyBuf, 1, privFileSz, privKeyFile) != privFileSz) {
            ret = WOLFCLU_FATAL_ERROR;
        }
    }

    /* convert PEM to DER if necessary */
    if (inForm == PEM_FORM && ret == 0) {
        ret = wolfCLU_KeyPemToDer(&keyBuf, privFileSz, 0);
        if (ret < 0) {
            wolfCLU_LogError("Failed to convert PEM to DER.\nRET: %d", ret);
        }
        else {
            privFileSz = ret;
            ret = 0;
        }
    }

    /* decode the private key from the DER-encoded input */
    if (ret == 0) {
        ret = wc_Ed25519PrivateKeyDecode(keyBuf, &index, &key, privFileSz);
        if (ret == 0) {
            /* Calculate the public key */
            ret = wc_ed25519_make_public(&key, key.p, ED25519_PUB_KEY_SIZE);
            if (ret == 0) {
                key.pubKeySet = 1;
            }
        }
        else {
            wolfCLU_LogError("Failed to import private key.\nRET: %d", ret);
        }
    }

    /* export public key */
    if (ret == 0) {
        if (outLen < ED25519_PUB_KEY_SIZE) {
            wolfCLU_LogError("Output buffer too small. Required: %d, Provided: %d",
                             ED25519_PUB_KEY_SIZE, outLen);
            ret = BUFFER_E;
        }
        else {
            outLen = ED25519_PUB_KEY_SIZE;
            ret = wc_ed25519_export_public(&key, outBuf, &outLen);
            if (ret != 0) {
                wolfCLU_LogError("Failed to export ED25519 public key.\nRET: %d", ret);
            }
        }
    }

    /* cleanup allocated resources */
    XFCLOSE(privKeyFile);

    if (keyBuf!= NULL) {
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }

    wc_ed25519_free(&key);

    /* expected ret == WOLFCLU_SUCCESS */
    return (ret >= 0) ? WOLFCLU_SUCCESS : ret;
#else
    return NOT_COMPILED_IN;
#endif
}
#endif

int wolfCLU_verify_signature(char* sig, char* hashFile, char* out,
                             char* keyPath, int keyType, int pubIn,
                             int inForm)
{
    int hSz = 0;
    int fSz;
    int ret = WOLFCLU_FATAL_ERROR;

    byte* hash = NULL;
    byte* data = NULL;
    XFILE h;
    XFILE f;

    if (sig == NULL) {
        return BAD_FUNC_ARG;
    }

    f = XFOPEN(sig, "rb");
    if (f == NULL) {
        wolfCLU_LogError("unable to open file %s", sig);
        return BAD_FUNC_ARG;
    }

    XFSEEK(f, 0, SEEK_END);
    fSz = (int)XFTELL(f);

    data = (byte*)XMALLOC(fSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (data == NULL) {
        XFCLOSE(f);
        return MEMORY_E;
    }
    if (XFSEEK(f, 0, SEEK_SET) != 0 || (int)XFREAD(data, 1, fSz, f) != fSz) {
        XFCLOSE(f);
        return WOLFCLU_FATAL_ERROR;
    }
    XFCLOSE(f);

    switch(keyType) {
        case RSA_SIG_VER:
            ret = wolfCLU_verify_signature_rsa(data, out, fSz, keyPath, pubIn, inForm);
            break;

        case ECC_SIG_VER:
            h = XFOPEN(hashFile,"rb");
            if (h == NULL) {
                wolfCLU_LogError("unable to open file %s", hashFile);
                ret = BAD_FUNC_ARG;
                break;
            }

            XFSEEK(h, 0, SEEK_END);
            hSz = (int)XFTELL(h);

            hash = (byte*)XMALLOC(hSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
            if (hash == NULL) {
                ret = MEMORY_E;
                XFCLOSE(h);
                break;
            }

            if (XFSEEK(h, 0, SEEK_SET) != 0 || (int)XFREAD(hash, 1, hSz, h) != hSz) {
                XFCLOSE(h);
                return WOLFCLU_FATAL_ERROR;
            }
            XFCLOSE(h);
            ret = wolfCLU_verify_signature_ecc(data, fSz, hash, hSz, keyPath,
                                               pubIn, inForm);
            break;

        case ED25519_SIG_VER:
        #ifdef HAVE_ED25519
            h = XFOPEN(hashFile, "rb");
            if (h == NULL) {
                wolfCLU_LogError("unable to open file %s", hashFile);
                ret = BAD_FUNC_ARG;
                break;
            }

            XFSEEK(h, 0, SEEK_END);
            hSz = (int)XFTELL(h);

            hash = (byte*)XMALLOC(hSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
            if (hash == NULL) {
                ret = MEMORY_E;
                XFCLOSE(h);
                break;
            }

            if (XFSEEK(h, 0, SEEK_SET) != 0 || (int)XFREAD(hash, 1, hSz, h) != hSz) {
                XFCLOSE(h);
                return WOLFCLU_FATAL_ERROR;
            }
            XFCLOSE(h);
            ret = wolfCLU_verify_signature_ed25519(data, fSz, hash, hSz,
                                                   keyPath, pubIn, inForm);
        #endif
            break;

#ifdef HAVE_DILITHIUM
        case DILITHIUM_SIG_VER:
            /* hashFIle means msgFile */
            h = XFOPEN(hashFile, "rb");
            if (h == NULL) {
                wolfCLU_LogError("unable to open file %s", hashFile);
                ret = BAD_FUNC_ARG;
                break;
            }

            /* hSz means msgLen */
            XFSEEK(h, 0, SEEK_END);
            hSz = (int)XFTELL(h);

            /* hash means msg */
            hash = (byte*)XMALLOC(hSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
            if (hash == NULL) {
                ret = MEMORY_E;
                XFCLOSE(h);
                break;
            }

            if (XFSEEK(h, 0, SEEK_SET) != 0 || (int)XFREAD(hash, 1, hSz, h) != hSz) {
                XFCLOSE(h);
                return WOLFCLU_FATAL_ERROR;
            }
            XFCLOSE(h);

            ret = wolfCLU_verify_signature_dilithium(data, fSz, hash, hSz, keyPath, inForm);
            break;
#endif

        default:
            wolfCLU_LogError("No valid verify algorithm selected.");
            ret = -1;
    }

    if (data != NULL) {
        XFREE(data , HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (hash != NULL) {
        XFREE(hash, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }

    return ret;
}

int wolfCLU_verify_signature_rsa(byte* sig, char* out, int sigSz, char* keyPath,
                                 int pubIn, int inForm) {

#ifndef NO_RSA
    int ret;
    int keyFileSz = 0;
    word32 index = 0;
    XFILE keyPathFile = NULL;
    RsaKey key;
    byte* keyBuf = NULL;
    byte* outBuf = NULL;
    int   outBufSz = 0;

    XMEMSET(&key, 0, sizeof(key));

    /* initialize RSA key */
    ret = wc_InitRsaKey(&key, NULL);
    if (ret != 0) {
        wolfCLU_LogError("Failed to initialize RsaKey.\nRet: %d", ret);
    }

    /* open, read, and store RSA key  */
    if (ret == 0) {
        keyPathFile = XFOPEN(keyPath, "rb");
        if (keyPathFile == NULL) {
            wolfCLU_LogError("unable to open file %s", keyPath);
            ret = BAD_FUNC_ARG;
        }
    }
    if (ret == 0) {
        XFSEEK(keyPathFile, 0, SEEK_END);
        keyFileSz = (int)XFTELL(keyPathFile);
        keyBuf = (byte*)XMALLOC(keyFileSz+1, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (keyBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(keyBuf, 0, keyFileSz+1);
        if (XFSEEK(keyPathFile, 0, SEEK_SET) != 0 ||
            (int)XFREAD(keyBuf, 1, keyFileSz, keyPathFile) != keyFileSz) {
            ret = WOLFCLU_FATAL_ERROR;
        }
    }

    /* convert PEM to DER if necessary */
    if (inForm == PEM_FORM && ret == 0) {
        ret = wolfCLU_KeyPemToDer(&keyBuf, keyFileSz, pubIn);
        if (ret < 0) {
            wolfCLU_LogError("Failed to convert PEM to DER.\nRET: %d", ret);
        }
        else {
            keyFileSz = ret;
            ret = 0;
        }
    }

    if (pubIn == 1) {
        /* decode public key from DER-encoded input */
        if (ret == 0) {
            ret = wc_RsaPublicKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret != 0) {
                wolfCLU_LogError("Failed to decode public key from DER.\nRET: %d", ret);
            }
        }
    }
    else {
        /* retrieve private key and store in the RsaKey */
        if (ret == 0) {
            ret = wc_RsaPrivateKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret != 0) {
                wolfCLU_LogError("Failed to decode private key.\nRET: %d", ret);
            }
        }
    }

    /* set up output buffer based on key size */
    if (ret == 0) {
        outBufSz = wc_RsaEncryptSize(&key);
        if (outBufSz <= 0) {
            wolfCLU_LogError("Invalid output buffer size: %d", outBufSz);
            ret = WOLFCLU_FATAL_ERROR;
        }
    }
    if (ret == 0) {
        outBuf = (byte*)XMALLOC(outBufSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (outBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(outBuf, 0, outBufSz);

        /* verify the RSA signature */
        ret = wc_RsaSSL_Verify(sig, sigSz, outBuf, (word32)outBufSz, &key);
        if (ret < 0) {
            wolfCLU_LogError("Failed to verify data with pub key.\nRET: %d", ret);
        }

        /* write the output to the specified file */
        XFILE s = XFOPEN(out, "wb");
        if (s == NULL) {
            wolfCLU_LogError("Unable to open file %s", out);
            ret = BAD_FUNC_ARG;
        }
        else {
            XFWRITE(outBuf, 1, ret, s);
            XFCLOSE(s);
        }
    }

    /* Cleanup allocated resources */
    XFCLOSE(keyPathFile);

    if (outBuf != NULL) {
        XFREE(outBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (keyBuf != NULL) {
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }

    wc_FreeRsaKey(&key);

    /* expected ret == WOLFCLU_SUCCESS */
    return (ret >= 0) ? WOLFCLU_SUCCESS : ret;
#else
    return NOT_COMPILED_IN;
#endif
}

int wolfCLU_verify_signature_ecc(byte* sig, int sigSz, byte* hash, int hashSz,
                                 char* keyPath, int pubIn, int inForm)
{

#ifdef HAVE_ECC
    int ret;
    int keyFileSz = 0;
    int stat = 0;
    word32 index = 0;

    XFILE keyPathFile = NULL;
    ecc_key key;
    byte* keyBuf = NULL;
    byte* outBuf = NULL;
    int outBufSz = 0;

    XMEMSET(&key, 0, sizeof(key));

    /* initialize Ecc key */
    ret = wc_ecc_init(&key);
    if (ret != 0) {
        wolfCLU_LogError("Failed to initialize ecc key.\nRet: %d", ret);
    }

    /* open, read, and store Ecc key  */
    if (ret == 0) {
        keyPathFile = XFOPEN(keyPath, "rb");
        if (keyPathFile == NULL) {
            wolfCLU_LogError("unable to open file %s", keyPath);
            ret = BAD_FUNC_ARG;
        }
    }
    if (ret == 0) {
        XFSEEK(keyPathFile, 0, SEEK_END);
        keyFileSz = (int)XFTELL(keyPathFile);
        keyBuf = (byte*)XMALLOC(keyFileSz+1, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (keyBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(keyBuf, 0, keyFileSz+1);
        if (XFSEEK(keyPathFile, 0, SEEK_SET) != 0 ||
            (int)XFREAD(keyBuf, 1, keyFileSz, keyPathFile) != keyFileSz) {
            ret = WOLFCLU_FATAL_ERROR;
        }
    }

    /* convert PEM to DER if necessary */
    if (inForm == PEM_FORM && ret == 0) {
        ret = wolfCLU_KeyPemToDer(&keyBuf, keyFileSz, pubIn);
        if (ret < 0) {
            wolfCLU_LogError("Failed to convert PEM to DER.\nRET: %d", ret);
        }
        else {
            keyFileSz = ret;
            ret = 0;
        }
    }

    if (pubIn == 1) {
        /* retrieve public key and store in the Ecc key */
        if (ret == 0) {
            ret = wc_EccPublicKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret < 0 ) {
                wolfCLU_LogError("Failed to decode public key.\nRET: %d", ret);
            }
        }
    }
    else {
        /* retrieve private key and store in the Ecc Key */
        if (ret == 0) {
            ret = wc_EccPrivateKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret != 0 ) {
                wolfCLU_LogError("Failed to decode Ecc private key.\nRET: %d", ret);
            }
        }
    }

    /* setting up output buffer based on key size */
    if (ret == 0) {
        outBufSz = wc_ecc_sig_size(&key);
        if (outBufSz <= 0) {
            wolfCLU_LogError("Invalid output buffer size: %d", outBufSz);
            ret = WOLFCLU_FATAL_ERROR;
        }
    }
    if (ret == 0) {
        outBuf = (byte*)XMALLOC(outBufSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (outBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(outBuf, 0, outBufSz);

        /* verify data with Ecc public key */
        ret = wc_ecc_verify_hash(sig, sigSz, hash, hashSz, &stat, &key);
        if (ret < 0) {
            wolfCLU_LogError("Failed to verify data with pub key.\nRET: %d", ret);
        }
        else if (stat == 1) {
            WOLFCLU_LOG(WOLFCLU_L0, "Valid Signature.");
        }
        else {
            wolfCLU_LogError("Invalid Signature.");
        }
    }

    /* cleanup allocated resources */
    XFCLOSE(keyPathFile);

    if (outBuf != NULL) {
        XFREE(outBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (keyBuf != NULL) {
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }

    wc_ecc_free(&key);

    /* expected ret == WOLFCLU_SUCCESS */
    return (ret >= 0) ? WOLFCLU_SUCCESS : ret;
#else
    return NOT_COMPILED_IN;
#endif
}

int wolfCLU_verify_signature_ed25519(byte* sig, int sigSz,
        byte* hash, int hashSz, char* keyPath, int pubIn, int inForm) {

#ifdef HAVE_ED25519
    int ret;
    int stat = 0;
    word32 index = 0;
    int keyFileSz = 0;

    XFILE keyPathFile = NULL;
    ed25519_key key;
    byte* keyBuf = NULL;

    XMEMSET(&key, 0, sizeof(key));

    /* initialize ED25519 key */
    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wolfCLU_LogError("Failed to initialize ED25519 key.\nRet: %d", ret);
    }

    /* open, read, and store ED25519 key */
    if (ret == 0) {
        keyPathFile = XFOPEN(keyPath, "rb");
        if (keyPathFile == NULL) {
            wolfCLU_LogError("unable to open file %s", keyPath);
            ret = BAD_FUNC_ARG;
        }
    }
    if (ret == 0) {
        XFSEEK(keyPathFile, 0, SEEK_END);
        keyFileSz = (int)XFTELL(keyPathFile);
        keyBuf = (byte*)XMALLOC(keyFileSz+1, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (keyBuf == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        XMEMSET(keyBuf, 0, keyFileSz+1);
        if (XFSEEK(keyPathFile, 0, SEEK_SET) != 0 ||
            (int)XFREAD(keyBuf, 1, keyFileSz, keyPathFile) != keyFileSz) {
            ret = WOLFCLU_FATAL_ERROR;
        }
    }

    /* convert PEM to DER if necessary */
    if (inForm == PEM_FORM && ret == 0) {
        ret = wolfCLU_KeyPemToDer(&keyBuf, keyFileSz, pubIn);
        if (ret < 0) {
            wolfCLU_LogError("Failed to convert PEM to DER.\nRET: %d", ret);
        }
        else {
            keyFileSz = ret;
            ret = 0;
        }
    }

    if (pubIn == 1 && ret == 0) {
        /* decode public key from RAW-encoded input */
        if (inForm == RAW_FORM) {
            ret = wc_ed25519_import_public(keyBuf, ED25519_KEY_SIZE, &key);
            if (ret != 0) {
                wolfCLU_LogError("Failed to import raw public key.\nRET: %d", ret);
            }
        }
        /* decode public key from DER-encoded input */
        else {
            ret = wc_Ed25519PublicKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret != 0) {
                wolfCLU_LogError("Failed to decode public key from DER.\nRET: %d", ret);
            }
        }
    }
    else if (ret == 0) {
        /* handle private key decoding and public key derivation */
        if (inForm == RAW_FORM) {
            ret = wc_ed25519_import_private_key(keyBuf,
                                                ED25519_KEY_SIZE,
                                                keyBuf + ED25519_KEY_SIZE,
                                                ED25519_KEY_SIZE, &key);
            if (ret < 0) {
                wolfCLU_LogError("Failed to decode private key.\nRET: %d", ret);
            }
        }
        else {
            ret = wc_Ed25519PrivateKeyDecode(keyBuf, &index, &key, keyFileSz);
            if (ret != 0) {
                wolfCLU_LogError("Failed to import private key.\nRET: %d", ret);
            }
        }

        /* calculate the public key */
        if (ret == 0) {
            ret = wc_ed25519_make_public(&key, key.p, ED25519_PUB_KEY_SIZE);
            if (ret == 0) {
                key.pubKeySet = 1;
            }
        }
    }

    /* verify data with ED25519 public key */
    if (ret == 0) {
        ret = wc_ed25519_verify_msg(sig, sigSz, hash, hashSz, &stat, &key);
        if (ret < 0) {
            wolfCLU_LogError("Failed to verify data with ED25519 public key.\nRET: %d", ret);
        }
        else if (stat == 1) {
            WOLFCLU_LOG(WOLFCLU_L0, "Valid Signature.");
        }
        else {
            wolfCLU_LogError("Invalid Signature.");
        }
    }

    /* cleanup allocated resources */
    XFCLOSE(keyPathFile);

    if (keyBuf != NULL) {
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }

    wc_ed25519_free(&key);

    /* expected ret == WOLFCLU_SUCCESS */
    return (ret >= 0) ? WOLFCLU_SUCCESS : ret;
#else
    return NOT_COMPILED_IN;
#endif  /* HAVE_ED25519 */
}

int wolfCLU_verify_signature_dilithium(byte* sig, int sigSz, byte* msg,
                    word32 msgLen, char* keyPath, int inForm)
{
#ifdef HAVE_DILITHIUM
    int ret = 0;

    XFILE keyFile = NULL;
    byte* keyBuf = NULL;
    int keyFileSz = 0;
    word32 keyBufSz = 0;
    word32 index = 0;
    int res = 0;

#ifdef WOLFSSL_SMALL_STACK
    dilithium_key* key;
    key = (dilithium_key*)XMALLOC(sizeof(dilithium_key), HEAP_HINT,
            DYNAMIC_TYPE_TMP_BUFFER);
    if (key == NULL) {
        return MEMORY_E;
    }
#else
    dilithium_key key[1];
#endif

    /* init the dilithium key */
    ret = wc_dilithium_init(key);
    if (ret != 0) {
        wolfCLU_LogError("Failed to initialize Dilithium Key.\nRET: %d", ret);
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return ret;
    }
    XMEMSET(key, 0, sizeof(dilithium_key));

    /* open and read public key */
    keyFile = XFOPEN(keyPath, "rb");
    if (keyFile == NULL) {
        wolfCLU_LogError("Faild to open Private key FILE.");
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return BAD_FUNC_ARG;
    }

    XFSEEK(keyFile, 0, SEEK_END);
    keyFileSz = (int)XFTELL(keyFile);
    keyBuf = (byte*)XMALLOC(keyFileSz, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (keyBuf == NULL) {
        wolfCLU_LogError("Failed to malloc key buffer.");
        XFCLOSE(keyFile);
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return MEMORY_E;
    }
    XMEMSET(keyBuf, 0, keyFileSz);

    if (XFSEEK(keyFile, 0, SEEK_SET) != 0 ||
        (int)XFREAD(keyBuf, 1, keyFileSz, keyFile) != keyFileSz) {
        wolfCLU_LogError("Failed to read public key.\nRET: %d", ret);
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return WOLFCLU_FATAL_ERROR;
    }
    keyBufSz = (word32)keyFileSz;
    XFCLOSE(keyFile);

    /* convert PEM to DER if necessary */
    if (inForm == PEM_FORM) {
        ret = wolfCLU_KeyPemToDer(&keyBuf, keyFileSz, 1);
        if (ret < 0) {
            wolfCLU_LogError("Failed to convert PEM to DER.\nRET: %d", ret);
            XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        #ifdef WOLFSSL_SMALL_STACK
            wc_dilithium_free(key);
        #endif
            return ret;
        }
        else {
            keyBufSz = ret;
        }
    }

    /* retrieving public key and storing in the dilithium key */
    ret = wc_Dilithium_PublicKeyDecode(keyBuf, &index, key, keyBufSz);
    if (ret != 0) {
        wolfCLU_LogError("Failed to decode public key.\nRET: %d", ret);
        XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return ret;
    }
    XFREE(keyBuf, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);

    /* verify the massage using the dilithium public key */
    ret = wc_dilithium_verify_msg(sig, sigSz, msg, msgLen, &res, key);
    if (ret != 0) {
        wolfCLU_LogError("Failed to verify data with Dilithium public key.\n"
                        "RET: %d", ret);
    #ifdef WOLFSSL_SMALL_STACK
        wc_dilithium_free(key);
    #endif
        return ret;
    }
    else if(res == 1) {
        WOLFCLU_LOG(WOLFCLU_L0, "Valid Signature.");
    }
    else {
        wolfCLU_LogError("Invalid Signature.");
    }

#ifdef WOLFSSL_SMALL_STACK
    wc_dilithium_free(key);
#endif

    return WOLFCLU_SUCCESS;
#else
    (void)sig;
    (void)sigSz;
    (void)msg;
    (void)msgLen;
    (void)keyPath;
    (void)inForm;

    return NOT_COMPILED_IN;
#endif  /* HAVE_DILITHIUM */
}

#endif /* WOLFCLU_NO_FILESYSTEM */
