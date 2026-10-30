// ctap2.cpp - minimal CTAP2 authenticator (ES256, attestation none).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#pragma comment(lib, "bcrypt.lib")

#include "ctap2.h"

struct Cbor {
    enum Type { Int, Bytes, Text, Array, Map, Bool, Nil } type = Int;
    int64_t i = 0;
    bool b = false;
    std::vector<uint8_t> bytes;
    std::string text;
    std::vector<Cbor> arr;
    std::vector<std::pair<Cbor, Cbor>> map;

    const Cbor* getInt(int64_t k) const {
        for (auto& kv : map)
            if (kv.first.type == Int && kv.first.i == k) return &kv.second;
        return nullptr;
    }
    const Cbor* getText(const char* k) const {
        for (auto& kv : map)
            if (kv.first.type == Text && kv.first.text == k) return &kv.second;
        return nullptr;
    }
};

static bool read_len(const uint8_t*& p, const uint8_t* e, uint8_t ai, uint64_t& n) {
    if (ai < 24) { n = ai; return true; }
    int bytes = 0;
    if (ai == 24) bytes = 1;
    else if (ai == 25) bytes = 2;
    else if (ai == 26) bytes = 4;
    else if (ai == 27) bytes = 8;
    else return false;
    if ((size_t)(e - p) < (size_t)bytes) return false;
    n = 0;
    for (int i = 0; i < bytes; ++i) n = (n << 8) | *p++;
    return true;
}

static bool decode(const uint8_t*& p, const uint8_t* e, Cbor& v, int depth = 0) {
    if (p >= e || depth > 16) return false;
    uint8_t ib = *p++;
    int major = ib >> 5;
    uint8_t ai = ib & 0x1F;
    uint64_t n = 0;

    switch (major) {
    case 0:
        if (!read_len(p, e, ai, n)) return false;
        v.type = Cbor::Int; v.i = (int64_t)n; return true;
    case 1:
        if (!read_len(p, e, ai, n)) return false;
        v.type = Cbor::Int; v.i = -1 - (int64_t)n; return true;
    case 2:
        if (!read_len(p, e, ai, n) || n > (uint64_t)(e - p)) return false;
        v.type = Cbor::Bytes; v.bytes.assign(p, p + n); p += n; return true;
    case 3:
        if (!read_len(p, e, ai, n) || n > (uint64_t)(e - p)) return false;
        v.type = Cbor::Text; v.text.assign((const char*)p, (size_t)n); p += n; return true;
    case 4:
        if (!read_len(p, e, ai, n) || n > 1024) return false;
        v.type = Cbor::Array; v.arr.resize((size_t)n);
        for (uint64_t i = 0; i < n; ++i) if (!decode(p, e, v.arr[(size_t)i], depth + 1)) return false;
        return true;
    case 5:
        if (!read_len(p, e, ai, n) || n > 1024) return false;
        v.type = Cbor::Map;
        for (uint64_t i = 0; i < n; ++i) {
            Cbor k, val;
            if (!decode(p, e, k, depth + 1) || !decode(p, e, val, depth + 1)) return false;
            v.map.emplace_back(std::move(k), std::move(val));
        }
        return true;
    case 7:
        if (ai == 20) { v.type = Cbor::Bool; v.b = false; return true; }
        if (ai == 21) { v.type = Cbor::Bool; v.b = true; return true; }
        if (ai == 22) { v.type = Cbor::Nil; return true; }
        return false;
    default:
        return false;
    }
}

struct Enc {
    std::vector<uint8_t> b;
    void byte(uint8_t x) { b.push_back(x); }
    void head(int major, uint64_t n) {
        if (n < 24) b.push_back((uint8_t)((major << 5) | n));
        else if (n <= 0xFF) { b.push_back((uint8_t)((major << 5) | 24)); byte((uint8_t)n); }
        else if (n <= 0xFFFF) { b.push_back((uint8_t)((major << 5) | 25)); byte((uint8_t)(n >> 8)); byte((uint8_t)n); }
        else { b.push_back((uint8_t)((major << 5) | 26)); for (int i = 3; i >= 0; --i) byte((uint8_t)(n >> (8 * i))); }
    }
    void map(size_t n) { head(5, n); }
    void array(size_t n) { head(4, n); }
    void text(const std::string& s) { head(3, s.size()); b.insert(b.end(), s.begin(), s.end()); }
    void bytes(const std::vector<uint8_t>& v) { head(2, v.size()); b.insert(b.end(), v.begin(), v.end()); }
    void integer(int64_t n) { if (n >= 0) head(0, (uint64_t)n); else head(1, (uint64_t)(-1 - n)); }
    void boolean(bool x) { byte(x ? 0xF5 : 0xF4); }
};

static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE h = nullptr;
    std::vector<uint8_t> out(32);
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return out;
    BCryptHash(h, nullptr, 0, (PUCHAR)data.data(), (ULONG)data.size(), out.data(), 32);
    BCryptCloseAlgorithmProvider(h, 0);
    return out;
}

static void put_u32(std::vector<uint8_t>& v, uint32_t n) {
    v.push_back((uint8_t)(n >> 24)); v.push_back((uint8_t)(n >> 16));
    v.push_back((uint8_t)(n >> 8)); v.push_back((uint8_t)n);
}

static const uint8_t k_aaguid[16] = {
    0x46,0x49,0x44,0x4F,0x32,0x56,0x49,0x52,
    0x54,0x55,0x41,0x4C,0x30,0x30,0x30,0x31
};

struct Key {
    BCRYPT_KEY_HANDLE h = nullptr;
    std::vector<uint8_t> x, y;
};

static bool make_key(Key& k) {
    static BCRYPT_ALG_HANDLE prov = nullptr;
    if (!prov && !BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&prov, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0)))
        return false;
    if (!BCRYPT_SUCCESS(BCryptGenerateKeyPair(prov, &k.h, 256, 0))) return false;
    if (!BCRYPT_SUCCESS(BCryptFinalizeKeyPair(k.h, 0))) return false;
    uint8_t blob[128] = {};
    ULONG len = 0;
    if (!BCRYPT_SUCCESS(BCryptExportKey(k.h, nullptr, BCRYPT_ECCPUBLIC_BLOB, blob, sizeof(blob), &len, 0)))
        return false;
    auto* hdr = (BCRYPT_ECCKEY_BLOB*)blob;
    if (hdr->cbKey != 32 || len < 8 + 64) return false;
    k.x.assign(blob + 8, blob + 40);
    k.y.assign(blob + 40, blob + 72);
    return true;
}

static std::vector<uint8_t> sign_raw(Key& k, const std::vector<uint8_t>& hash) {
    uint8_t sig[64] = {};
    ULONG len = 0;
    if (!BCRYPT_SUCCESS(BCryptSignHash(k.h, nullptr, (PUCHAR)hash.data(), (ULONG)hash.size(),
                                      sig, sizeof(sig), &len, 0)))
        return {};
    return std::vector<uint8_t>(sig, sig + len);
}

static void der_int(std::vector<uint8_t>& out, const uint8_t* p, size_t n) {
    while (n > 1 && p[0] == 0) { p++; n--; }
    bool pad = (p[0] & 0x80) != 0;
    out.push_back(0x02);
    out.push_back((uint8_t)(n + (pad ? 1 : 0)));
    if (pad) out.push_back(0);
    out.insert(out.end(), p, p + n);
}

static std::vector<uint8_t> der_sig(const std::vector<uint8_t>& raw) {
    if (raw.size() != 64) return {};
    std::vector<uint8_t> body;
    der_int(body, raw.data(), 32);
    der_int(body, raw.data() + 32, 32);
    std::vector<uint8_t> out;
    out.push_back(0x30);
    out.push_back((uint8_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

struct Cred {
    std::vector<uint8_t> id;
    std::string rp;
    std::vector<uint8_t> user;
    Key key;
    uint32_t count = 0;
};
static std::vector<Cred> g_creds;

static std::vector<uint8_t> cose(const Key& k) {
    Enc e;
    e.map(5);
    e.integer(1); e.integer(2);   // kty EC2
    e.integer(3); e.integer(-7);  // alg ES256
    e.integer(-1); e.integer(1);  // crv P-256
    e.integer(-2); e.bytes(k.x);
    e.integer(-3); e.bytes(k.y);
    return e.b;
}

static std::vector<uint8_t> get_info() {
    Enc e;
    e.byte(0x00);
    e.map(6);
    e.text("versions"); e.array(1); e.text("FIDO_2_0");
    e.text("extensions"); e.array(0);
    e.text("aaguid"); e.bytes(std::vector<uint8_t>(k_aaguid, k_aaguid + 16));
    e.text("options"); e.map(3);
    e.text("rk"); e.boolean(false);
    e.text("up"); e.boolean(true);
    e.text("uv"); e.boolean(false);
    e.text("maxMsgSize"); e.integer(1024);
    e.text("pinProtocols"); e.array(0);
    return e.b;
}

static std::vector<uint8_t> make_credential(const Cbor& req) {
    auto* cdh = req.getInt(1);
    auto* rp = req.getInt(2);
    auto* user = req.getInt(3);
    if (!cdh || cdh->type != Cbor::Bytes || cdh->bytes.size() != 32) return { 0x01 };
    if (!rp || rp->type != Cbor::Map) return { 0x02 };
    auto* rpId = rp->getText("id");
    if (!rpId || rpId->type != Cbor::Text) return { 0x02 };

    Cred c;
    c.rp = rpId->text;
    c.user = (user && user->type == Cbor::Map) ? std::vector<uint8_t>() : std::vector<uint8_t>();
    if (user && user->type == Cbor::Map) {
        auto* uid = user->getText("id");
        if (uid && uid->type == Cbor::Bytes) c.user = uid->bytes;
    }
    if (!make_key(c.key)) return { 0x01 };

    c.id.resize(32);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, c.id.data(), 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return { 0x01 };

    std::vector<uint8_t> auth;
    auto rpHash = sha256(std::vector<uint8_t>(c.rp.begin(), c.rp.end()));
    auth.insert(auth.end(), rpHash.begin(), rpHash.end());
    auth.push_back(0x41); // UP + AT
    put_u32(auth, 0);
    auth.insert(auth.end(), k_aaguid, k_aaguid + 16);
    auth.push_back(0x00); auth.push_back((uint8_t)c.id.size());
    auth.insert(auth.end(), c.id.begin(), c.id.end());
    auto pub = cose(c.key);
    auth.insert(auth.end(), pub.begin(), pub.end());

    g_creds.push_back(std::move(c));

    Enc e;
    e.byte(0x00);
    e.map(3);
    e.integer(1); e.text("none");
    e.integer(2); e.bytes(auth);
    e.integer(3); e.map(0);
    return e.b;
}

static std::vector<uint8_t> get_assertion(const Cbor& req) {
    auto* rp = req.getInt(1);
    auto* cdh = req.getInt(2);
    if (!rp || rp->type != Cbor::Text || !cdh || cdh->type != Cbor::Bytes || cdh->bytes.size() != 32)
        return { 0x01 };

    std::vector<std::vector<uint8_t>> allow;
    auto* al = req.getInt(3);
    if (al && al->type == Cbor::Array) {
        for (auto& d : al->arr) {
            if (d.type != Cbor::Map) continue;
            auto* id = d.getText("id");
            if (id && id->type == Cbor::Bytes) allow.push_back(id->bytes);
        }
    }

    Cred* found = nullptr;
    for (auto& c : g_creds) {
        if (c.rp != rp->text) continue;
        if (!allow.empty()) {
            bool ok = false;
            for (auto& a : allow) if (a == c.id) ok = true;
            if (!ok) continue;
        }
        found = &c;
        break;
    }
    if (!found) return { 0x2B };

    std::vector<uint8_t> auth;
    auto rpHash = sha256(std::vector<uint8_t>(found->rp.begin(), found->rp.end()));
    auth.insert(auth.end(), rpHash.begin(), rpHash.end());
    auth.push_back(0x01); // UP
    put_u32(auth, ++found->count);

    std::vector<uint8_t> toSign = auth;
    toSign.insert(toSign.end(), cdh->bytes.begin(), cdh->bytes.end());
    auto hash = sha256(toSign);
    auto raw = sign_raw(found->key, hash);
    auto sig = der_sig(raw);
    if (sig.empty()) return { 0x01 };

    Enc cred;
    cred.map(2);
    cred.text("type"); cred.text("public-key");
    cred.text("id"); cred.bytes(found->id);

    Enc e;
    e.byte(0x00);
    e.map(4);
    e.integer(1); e.b.insert(e.b.end(), cred.b.begin(), cred.b.end());
    e.integer(2); e.bytes(auth);
    e.integer(3); e.bytes(sig);
    e.integer(4); e.map(1);
    e.text("id"); e.bytes(found->user);
    return e.b;
}

std::vector<uint8_t> ctap2_handle(uint8_t cmd, const uint8_t* data, size_t len) {
    if (cmd == 0x04) return get_info();
    Cbor req;
    const uint8_t* p = data;
    const uint8_t* e = data + len;
    if (!decode(p, e, req)) return { 0x01 };
    if (cmd == 0x01) return make_credential(req);
    if (cmd == 0x02) return get_assertion(req);
    return { 0x01 };
}
