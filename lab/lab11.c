#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#define MAX_BUF 4096

int verify(const char* message_path, const char* sign_path, EVP_PKEY* pkey) {
    unsigned char message[MAX_BUF];
    unsigned char signature[MAX_BUF];
    size_t message_len = 0;
    size_t signature_len = 0;

    // 1. Read message file
    FILE* msg_f = fopen(message_path, "rb");
    if (!msg_f) return -1;
    message_len = fread(message, 1, MAX_BUF, msg_f);
    fclose(msg_f);

    // 2. Read signature file
    FILE* sig_f = fopen(sign_path, "rb");
    if (!sig_f) return -1;
    signature_len = fread(signature, 1, MAX_BUF, sig_f);
    fclose(sig_f);

    // 3. Setup OpenSSL Context (Like we did in blockchain)
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) return -1;

    int result = 0; // Assume fail

    // 4. Initialize, Update, and Finalize Verification
    // EVP_sha256() tells it what algorithm was used to sign
    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) == 1) {
        if (EVP_DigestVerifyUpdate(mdctx, message, message_len) == 1) {
            // EVP_DigestVerifyFinal returns 1 for valid signature, 0 for mismatch
            if (EVP_DigestVerifyFinal(mdctx, signature, signature_len) == 1) {
                result = 1; // Success!
            }
        }
    }

    // 5. Cleanup
    EVP_MD_CTX_free(mdctx);
    return result; 
}

int main() {
    // 1. Load the Public Key
    FILE* pub_f = fopen("public_key.pem", "r");
    if (!pub_f) {
        perror("Could not open public_key.pem");
        return 1;
    }

    // PEM_read_PUBKEY handles the heavy lifting of reading the key file
    EVP_PKEY* pkey = PEM_read_PUBKEY(pub_f, NULL, NULL, NULL);
    fclose(pub_f);

    if (!pkey) {
        fprintf(stderr, "Error loading public key\n");
        return 1;
    }

    // 2. Verify the three sets of files
    char* msgs[] = {"message1.txt", "message2.txt", "message3.txt"};
    char* sigs[] = {"signature1.sig", "signature2.sig", "signature3.sig"};

    for (int i = 0; i < 3; i++) {
        if (verify(msgs[i], sigs[i], pkey)) {
            printf("%s: VERIFIED (TRUTH)\n", msgs[i]);
        } else {
            printf("%s: FAILED (LIE)\n", msgs[i]);
        }
    }

    EVP_PKEY_free(pkey);
    return 0;
}
