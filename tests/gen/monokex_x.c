#include <sodium.h>
#include "utils.h"

static const uint8_t zero[32] = {0};
static const uint8_t one [16] = {1};

static void crypto_chacha20_H(uint8_t       out[32],
                              const uint8_t key[32],
                              const uint8_t in [16])
{
    crypto_core_hchacha20(out, in, key, 0);
}
int crypto_x25519(uint8_t       shared[32],
                  const uint8_t sk    [32],
                  const uint8_t pk    [32])
{
    return crypto_scalarmult(shared, sk, pk);
}
#define crypto_x25519_public_key crypto_scalarmult_base
#define crypto_poly1305_ctx      crypto_onetimeauth_state
#define crypto_poly1305_init     crypto_onetimeauth_init
#define crypto_poly1305_update   crypto_onetimeauth_update
#define crypto_poly1305_final    crypto_onetimeauth_final

static void xor(uint8_t out[32], const uint8_t a[32], const uint8_t b[32])
{
    for (unsigned i = 0; i < 32; i++) {
        out[i] = a[i] ^ b[i];
    }
}

static void copy(uint8_t out[32], const uint8_t in[32])
{
    for (unsigned i = 0; i < 32; i++) {
        out[i] = in[i];
    }
}

static void chacha_block(uint8_t       out[64],
                         const uint8_t key[32],
                         const uint8_t nonce[16])
{
    static const uint8_t in[64] = {0};
    crypto_stream_chacha20_xor_ic(out, in, 64, nonce, 0, key);
}

typedef struct {
    // Key pairs
    uint8_t is[32];  uint8_t IS[32];
    uint8_t ie[32];  uint8_t IE[32];
    uint8_t rs[32];  uint8_t RS[32];

    // Shared secrets
    uint8_t es[32];
    uint8_t ss[32];

    // Symmetric Keys
    uint8_t CK1[32];
    uint8_t CK2[32];
    uint8_t AK2[32];
    uint8_t EK1[32];
    uint8_t EK2[32];

    // Messages
    uint8_t msg1[80];
} test_vectors_x;

static void vectors_x_fill(test_vectors_x *v,
                           const uint8_t client_sk  [32],
                           const uint8_t server_sk  [32],
                           const uint8_t client_seed[32])
{
    // Private keys
    copy(v->is, client_sk  );
    copy(v->ie, client_seed);
    copy(v->rs, server_sk  );

    // Public keys
    crypto_x25519_public_key(v->IS, v->is);
    crypto_x25519_public_key(v->IE, v->ie);
    crypto_x25519_public_key(v->RS, v->rs);

    // Exchanges
    crypto_x25519(v->es, v->ie, v->RS);
    crypto_x25519(v->ss, v->is, v->RS);

    // Keys
    uint8_t tmp1[32];
    uint8_t tmp2[32];
    uint8_t CK0 [32] = "Monokex X";
    crypto_chacha20_H(tmp1, v->es , zero);
    crypto_chacha20_H(tmp2, CK0   , one );
    xor(v->CK1, tmp1, tmp2);
    crypto_chacha20_H(tmp1, v->ss , zero);
    crypto_chacha20_H(tmp2, v->CK1, one );
    xor(v->CK2, tmp1, tmp2);
    uint8_t tmp[64];
    chacha_block(tmp, v->CK1, one);
    copy(v->EK1, tmp + 32);
    chacha_block(tmp, v->CK2, one);
    copy(v->AK2, tmp     );
    copy(v->EK2, tmp + 32);

    // Messages
    crypto_poly1305_ctx ctx;
    uint8_t XIS[32];
    xor(XIS, v->IS, v->EK1);
    copy(v->msg1     , v->IE);
    copy(v->msg1 + 32, XIS  );
    crypto_poly1305_init  (&ctx, v->AK2);
    crypto_poly1305_update(&ctx, v->RS, 32);
    crypto_poly1305_update(&ctx, v->IE, 32);
    crypto_poly1305_update(&ctx, XIS  , 32);
    crypto_poly1305_final (&ctx, v->msg1 + 64);
}

int main(void)
{
    FOR(i, 0, 5) {
        RANDOM_INPUT(client_sk  , 32);
        RANDOM_INPUT(server_sk  , 32);
        RANDOM_INPUT(client_seed, 32);
        RANDOM_INPUT(server_seed, 32);
        test_vectors_x v;
        vectors_x_fill(&v, client_sk, server_sk, client_seed);

        print_vector(v.is  , 32);
        print_vector(v.ie  , 32);
        print_vector(v.rs  , 32);
        print_vector(v.IS  , 32);
        print_vector(v.RS  , 32);
        print_vector(v.msg1, 80);
        print_vector(v.EK2 , 32);
    }
    return 0;
}
