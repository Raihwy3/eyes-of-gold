/* One-time ROM preparation. No baked meshes, draw commands or pixels live in
 * this executable. Rendering loads immutable results before workers start. */
#include "native_derived.h"
#include "native_inflate.h"
#include "native_stage_limits.h"
#include "native_setup_lod.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>
static constexpr char render_magic[]="EOGDRW21";
static_assert(sizeof(render_magic)==9,"Rendering-cache magic must occupy eight bytes");
using Bytes = std::vector<uint8_t>;
// Read only the asset currently being converted. Keeping the entire cartridge
// alongside accumulated meshes exceeded the 24 MiB application heap.
struct RomReader {
  FILE *file = nullptr;
  ~RomReader() { if (file) fclose(file); }
  bool open(const char *path) {
    file = fopen(path, "rb");
    if (!file) return false;
    std::array<uint8_t, 4096> block;
    unsigned hash = 2166136261u;
    for (unsigned at = 0; at < 0xc00000u; at += block.size()) {
      if (fread(block.data(), 1, block.size(), file) != block.size()) return false;
      for (auto byte : block) hash = (hash ^ byte) * 16777619u;
    }
    return fgetc(file) == EOF && !ferror(file) && hash == 2571402921u;
  }
  bool read(unsigned offset, unsigned count, Bytes &bytes) const {
    if (offset > 0xc00000u || count > 0xc00000u - offset ||
        fseek(file, offset, SEEK_SET)) return false;
    bytes.resize(count);
    return fread(bytes.data(), 1, count, file) == count && !ferror(file);
  }
};
#ifdef __3DS__
extern "C" void sysFatalError(const char *, ...) __attribute__((noreturn));
extern "C" void native_derived_memory_init(void) {
  std::set_new_handler([] {
    sysFatalError("Not enough application memory to prepare/load game assets");
  });
}
#endif
using Input = OfflineMeshLod::Input;
using Tri = OfflineMeshLod::Triangle;
struct ModelLayout {
  unsigned offset, packed, root, prop, proxy;
};
struct BgLayout {unsigned offset,bytes,rooms;};
#include "native_derived_layout.h"
struct TitleRange {
  unsigned offset, bytes, kind;
  unsigned char *destination;
};
#include "native_title_layout.h"
struct Huft {
  uint8_t e, b;
  union {
    uint16_t n;
    Huft *t;
  } v;
};
extern "C" uint32_t decompressdata(void *, void *, Huft *);
NativeBakedLodRecord *native_baked_lods;
NativeRoomLodRecord *native_room_lods;
unsigned native_baked_lod_count, native_room_lod_count;
unsigned char *original_bottom_background;
NativeMeshLod::Entry *native_bg_lods;
NativeSceneryBatches::Entry *native_room_packets;
unsigned native_bg_lod_count, native_room_packet_count;
static NativeConvertProgress setup_progress;
static const char *setup_stage = "Preparing rendering data";
static unsigned setup_done, setup_total;
static bool setup_cancelled;
bool native_setup_lod_progress() {
  if (setup_cancelled)
    return false;
  if (setup_progress && !setup_progress(setup_stage, setup_done, setup_total))
    setup_cancelled = true;
  return !setup_cancelled;
}
namespace {
constexpr unsigned max_payload = 32 * 1024 * 1024;
unsigned char *mounted;
uint32_t fnv(const void *p, size_t size, uint32_t h = 2166136261u) {
  const auto *b = (const uint8_t *)p;
  while (size--)
    h = (h ^ *b++) * 16777619u;
  return h;
}
unsigned be16(const uint8_t *p) { return unsigned(p[0]) * 256 + p[1]; }
unsigned be32(const uint8_t *p) {
  return unsigned(p[0]) << 24 | unsigned(p[1]) << 16 | unsigned(p[2]) << 8 |
         p[3];
}
unsigned le32(const uint8_t *p) {
  unsigned n;
  memcpy(&n, p, 4);
  return n;
}
bool range(size_t size, size_t at, size_t n) {
  return at <= size && n <= size - at;
}
void append(Bytes &b, const void *p, size_t n) {
  auto *src = (const uint8_t *)p;
  b.insert(b.end(), src, src + n);
}
void word(Bytes &b, unsigned n) { append(b, &n, 4); }
void native_vertices(Bytes &b) {
  for (size_t i = 0; i + 16 <= b.size(); i += 16)
    for (unsigned j = 0; j < 12; j += 2)
      std::swap(b[i + j], b[i + j + 1]);
}
Bytes inflate_blob(const uint8_t *src, size_t size) {
  if (size < 2 || src[0] != 0x11 || src[1] != 0x72 || size > 0xc00000u)
    return {};
  constexpr unsigned capacity = 2 * 1024 * 1024;
  // Original inflater's overlap guard requires input above the output extent.
  Bytes workspace(capacity + 64 + size + 16);
  auto *input = workspace.data() + capacity + 64;
  memcpy(input, src, size);
  std::vector<Huft> hlist(NATIVE_HUFT_ENTRIES);
  unsigned n = decompressdata(input, workspace.data(), hlist.data());
  if (n > capacity)
    return {};
  // resize() alone retains the 2 MiB decompression capacity for every live
  // room stream. Return only the actual decoded bytes.
  return Bytes(workspace.begin(), workspace.begin() + n);
}
bool notify(NativeConvertProgress cb, const char *text, unsigned at,
            unsigned total) {
  setup_progress = cb;
  setup_stage = text;
  setup_done = at;
  setup_total = total;
  return native_setup_lod_progress();
}
struct Node {
  unsigned op, data;
};
std::vector<Node> nodes(const Bytes &raw, unsigned root) {
  std::vector<unsigned> pending{root};
  std::set<unsigned> seen;
  std::vector<Node> out;
  while (!pending.empty()) {
    unsigned at = pending.back();
    pending.pop_back();
    if (!range(raw.size(), at, 24) || !seen.insert(at).second)
      continue;
    unsigned op = be16(raw.data() + at) & 255,
             data = be32(raw.data() + at + 4) & 0xffffff;
    out.push_back({op, data});
    unsigned child = op == 8    ? (range(raw.size(), data + 8, 4)
                                       ? be32(raw.data() + data + 8) & 0xffffff
                                       : 0)
                     : op == 18 ? (range(raw.size(), data, 4)
                                       ? be32(raw.data() + data) & 0xffffff
                                       : 0)
                                : be32(raw.data() + at + 20) & 0xffffff;
    if (child)
      pending.push_back(child);
    unsigned next = be32(raw.data() + at + 12) & 0xffffff;
    if (next)
      pending.push_back(next);
  }
  return out;
}
std::vector<std::array<unsigned, 3>> triangles(unsigned x, unsigned y) {
  std::vector<std::array<unsigned, 3>> t;
  if (x >> 24 == 0xbf)
    t.push_back({(y >> 16 & 255) / 10, (y >> 8 & 255) / 10, (y & 255) / 10});
  if (x >> 24 == 0xb1)
    for (unsigned j = 0; j < 4; j++) {
      unsigned a = y >> (8 * j) & 15, b = y >> (8 * j + 4) & 15,
               c = x >> (4 * j) & 15;
      if (a || b || c)
        t.push_back({a, b, c});
    }
  return t;
}
struct Job {
  unsigned hash = 0, room = 0, proxy = 0, mixed = 0;
  Bytes vertices;
  std::vector<Input> points;
  std::vector<Tri> tri;
  std::vector<int> original;
  std::vector<unsigned> domain;
  std::set<unsigned> protected_points;
};
std::array<int, 4> point_key(const uint8_t *p, unsigned domain) {
  return {(int)domain, (int16_t)be16(p), (int16_t)be16(p + 2),
          (int16_t)be16(p + 4)};
}
unsigned add_point(Job &job, std::map<std::array<int, 4>, unsigned> &ids,
                   const uint8_t *p, unsigned domain) {
  auto key = point_key(p, domain);
  auto found = ids.find(key);
  if (found != ids.end())
    return found->second;
  Input v{};
  for (unsigned k = 0; k < 3; k++)
    v.xyz[k] = (int16_t)key[k + 1];
  memset(v.rgba, 127, 4);
  unsigned id = job.points.size();
  ids[key] = id;
  job.points.push_back(v);
  job.domain.push_back(domain);
  return id;
}
std::vector<Job> model_jobs(const Bytes &raw, const ModelLayout &layout) {
  auto ns = nodes(raw, layout.root);
  if (layout.prop &&
      std::any_of(ns.begin(), ns.end(), [](auto n) { return n.op == 8; }))
    return {};
  std::vector<Job> result;
  for (auto node : ns) {
    if (node.op != 24 || !range(raw.size(), node.data, 28))
      continue;
    unsigned dl = be32(raw.data() + node.data) & 0xffffff,
             vo = be32(raw.data() + node.data + 8) & 0xffffff,
             nv = be16(raw.data() + node.data + 12);
    if (!dl || !vo || !nv || nv > 2048 || !range(raw.size(), vo, nv * 16))
      continue;
    Job job;
    job.proxy = layout.proxy;
    job.vertices.assign(raw.begin() + vo, raw.begin() + vo + nv * 16);
    native_vertices(job.vertices);
    job.original.assign(nv, -1);
    std::map<std::array<int, 4>, unsigned> ids;
    int loaded[128];
    std::fill_n(loaded, 128, -1);
    unsigned matrix = 0;
    bool valid = true;
    for (unsigned at = dl; range(raw.size(), at, 8); at += 8) {
      unsigned x = be32(raw.data() + at), y = be32(raw.data() + at + 4),
               op = x >> 24;
      if (op == 0xb8)
        break;
      if (op == 1)
        matrix = y;
      if (op == 6) {
        valid = false;
        break;
      }
      if (op == 4) {
        unsigned count = (x & 65535) / 16, dest = x >> 16 & 15,
                 offset = y & 0xffffff;
        if (y >> 24 != 4 || offset % 16 || offset / 16 + count > nv ||
            dest + count > 128) {
          valid = false;
          break;
        }
        for (unsigned i = 0; i < count; i++) {
          unsigned index = offset / 16 + i,
                   id = add_point(job, ids, raw.data() + vo + index * 16,
                                  matrix);
          if (job.original[index] >= 0 && job.original[index] != (int)id)
            valid = false;
          job.original[index] = id;
          loaded[dest + i] = id;
        }
      }
      for (auto t : triangles(x, y)) {
        if (t[0] >= 128 || t[1] >= 128 || t[2] >= 128 || loaded[t[0]] < 0 ||
            loaded[t[1]] < 0 || loaded[t[2]] < 0) {
          valid = false;
          break;
        }
        unsigned a = loaded[t[0]], b = loaded[t[1]], c = loaded[t[2]];
        if (job.domain[a] != job.domain[b] || job.domain[a] != job.domain[c]) {
          job.protected_points.insert({a, b, c});
          job.mixed++;
          continue;
        }
        job.tri.push_back({(uint16_t)a, (uint16_t)b, (uint16_t)c});
      }
      if (!valid)
        break;
    }
    if (!valid || job.tri.empty() || job.tri.size() > 2048 ||
        job.points.size() > 2048)
      continue;
    job.hash = fnv(job.vertices.data(), job.vertices.size());
    result.push_back(std::move(job));
  }
  return result;
}
// First-launch workspace on the heap, reused across all meshes. No topology
// work, dynamic allocation or SD reads are introduced into live rendering.
struct Workspace {
  Input points[2048];
  Tri tri[3][2048];
  unsigned map[3][2048];
};
unsigned surviving(const Job &job, const std::vector<unsigned> &map) {
  unsigned n = 0;
  for (auto t : job.tri) {
    const auto *a = job.points[map[t.a]].xyz, *b = job.points[map[t.b]].xyz,
               *c = job.points[map[t.c]].xyz;
    int64_t ab[3], ac[3];
    for (unsigned k = 0; k < 3; k++) {
      ab[k] = int(b[k]) - a[k];
      ac[k] = int(c[k]) - a[k];
    }
    if (ab[1] * ac[2] != ab[2] * ac[1] || ab[2] * ac[0] != ab[0] * ac[2] ||
        ab[0] * ac[1] != ab[1] * ac[0])
      n++;
  }
  return n;
}
std::vector<unsigned> proxy_map(const Job &job, unsigned cells) {
  unsigned n = job.points.size();
  std::vector<unsigned> parent(n), map(n);
  for (unsigned i = 0; i < n; i++)
    parent[i] = map[i] = i;
  auto root = [&](unsigned i) {
    while (parent[i] != i) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  for (auto t : job.tri) {
    parent[root(t.a)] = root(t.b);
    parent[root(t.b)] = root(t.c);
  }
  std::map<unsigned, std::vector<unsigned>> components;
  for (unsigned i = 0; i < n; i++)
    components[root(i)].push_back(i);
  for (auto &item : components) {
    auto &ids = item.second;
    if (ids.size() < 8)
      continue;
    int low[3] = {32767, 32767, 32767}, high[3] = {-32768, -32768, -32768};
    for (auto i : ids)
      for (unsigned k = 0; k < 3; k++) {
        low[k] = std::min(low[k], int(job.points[i].xyz[k]));
        high[k] = std::max(high[k], int(job.points[i].xyz[k]));
      }
    std::map<std::array<unsigned, 3>, std::vector<unsigned>> buckets;
    for (auto i : ids) {
      std::array<unsigned, 3> key;
      for (unsigned k = 0; k < 3; k++)
        key[k] = std::min(cells - 1,
                          unsigned((job.points[i].xyz[k] - low[k]) * cells /
                                   std::max(1, high[k] - low[k])));
      buckets[key].push_back(i);
    }
    for (auto &bucket : buckets) {
      unsigned best = UINT_MAX;
      double score = 1e99;
      for (auto i : bucket.second) {
        double value = 0;
        for (unsigned k = 0; k < 3; k++) {
          double target = low[k] + double(high[k] - low[k]) * bucket.first[k] /
                                       (cells - 1),
                 d = (job.points[i].xyz[k] - target) /
                     std::max(1, high[k] - low[k]);
          value += d * d;
        }
        if (value < score || (value == score && i < best)) {
          score = value;
          best = i;
        }
      }
      for (auto i : bucket.second)
        map[i] = best;
    }
  }
  return map;
}
struct Coordinates {
  unsigned hash, room, count;
  std::array<std::vector<int16_t>, 3> xyz;
};
bool reduce_job(const Job &job, Workspace &w, Coordinates &out) {
  memset(w.points, 0, sizeof w.points);
  std::copy(job.points.begin(), job.points.end(), w.points);
  for (auto i : job.protected_points)
    w.points[i].flag |= 0x8000;
  unsigned n = job.tri.size();
  unsigned mid = OfflineMeshLod::simplify(job.tri.data(), n, w.points, w.tri[0],
                                          1, w.map[0], job.room != 0);
  unsigned far = OfflineMeshLod::simplify(w.tri[0], mid, w.points, w.tri[1], 2,
                                          w.map[1], job.room != 0);
  unsigned distant = OfflineMeshLod::simplify(w.tri[1], far, w.points, w.tri[2],
                                              3, w.map[2], job.room != 0);
  std::array<std::vector<unsigned>, 3> maps;
  for (auto &m : maps)
    m.resize(job.points.size());
  for (unsigned i = 0; i < job.points.size(); i++) {
    maps[0][i] = w.map[0][i];
    maps[1][i] = w.map[1][w.map[0][i]];
    maps[2][i] = w.map[2][w.map[1][w.map[0][i]]];
  }
  if (job.proxy && job.protected_points.empty()) {
    auto a = proxy_map(job, 3), b = proxy_map(job, 2);
    unsigned x = surviving(job, a), y = surviving(job, b);
    if (y > 0 && y <= x && x < far && x <= mid) {
      maps[1] = std::move(a);
      maps[2] = std::move(b);
      far = x;
      distant = y;
    }
  }
  if (mid == n && far == n && distant == n)
    return false;
  out = {job.hash, job.room, unsigned(job.vertices.size() / 16), {}};
  for (unsigned tier = 0; tier < 3; tier++)
    for (unsigned i = 0; i < job.original.size(); i++) {
      int original = job.original[i];
      const int16_t *p = original >= 0
                             ? job.points[maps[tier][original]].xyz
                             : (const int16_t *)(job.vertices.data() + i * 16);
      for (unsigned k = 0; k < 3; k++)
        out.xyz[tier].push_back(p[k]);
    }
  return true;
}
struct WordLess {
  bool operator()(const std::vector<NativeSceneryBatches::Word> &a,
                  const std::vector<NativeSceneryBatches::Word> &b) const {
    for (unsigned i = 0; i < std::min(a.size(), b.size()); i++) {
      if (a[i].w0 != b[i].w0)
        return a[i].w0 < b[i].w0;
      if (a[i].w1 != b[i].w1)
        return a[i].w1 < b[i].w1;
    }
    return a.size() < b.size();
  }
};
struct Results {
  std::vector<Coordinates> objects, rooms;
  std::vector<NativeMeshLod::Entry> bg;
  std::set<std::vector<NativeSceneryBatches::Word>, WordLess> packets;
  Bytes backdrop, title;
};
Job room_job(unsigned room, const Bytes &vertices, const Bytes &dl) {
  Job job;
  job.room = room;
  job.vertices = vertices;
  native_vertices(job.vertices);
  job.hash = fnv(job.vertices.data(), job.vertices.size());
  job.original.resize(vertices.size() / 16);
  std::map<std::array<int, 4>, unsigned> ids;
  for (unsigned i = 0; i < job.original.size(); i++)
    job.original[i] = add_point(job, ids, vertices.data() + 16 * i, 0);
  int loaded[128];
  std::fill_n(loaded, 128, -1);
  bool valid = true;
  for (unsigned at = 0; range(dl.size(), at, 8); at += 8) {
    unsigned x = be32(dl.data() + at), y = be32(dl.data() + at + 4),
             op = x >> 24;
    if (op == 0xb8)
      break;
    if (op == 1 || op == 6 || op == 0xb2) {
      valid = false;
      break;
    }
    if (op == 4) {
      unsigned n = (x & 65535) / 16, dest = x >> 16 & 15, offset = y & 0xffffff;
      if (y >> 24 != 14 || offset % 16 || dest + n > 128 ||
          !range(vertices.size(), offset, n * 16)) {
        valid = false;
        break;
      }
      for (unsigned i = 0; i < n; i++)
        loaded[dest + i] = job.original[offset / 16 + i];
    }
    for (auto t : triangles(x, y)) {
      if (t[0] >= 128 || t[1] >= 128 || t[2] >= 128 || loaded[t[0]] < 0 ||
          loaded[t[1]] < 0 || loaded[t[2]] < 0) {
        valid = false;
        break;
      }
      job.tri.push_back({(uint16_t)loaded[t[0]], (uint16_t)loaded[t[1]],
                         (uint16_t)loaded[t[2]]});
    }
    if (!valid)
      break;
  }
  if (!valid || job.tri.size() > 2048 || job.points.size() > 2048)
    job.tri.clear();
  return job;
}
void bg_runs(Results &r, const Bytes &vertices, const Bytes &dl,
             std::set<Bytes> &seen) {
  NativeMeshLod::Input loaded[128]{};
  bool available[128]{};
  std::vector<NativeSceneryBatches::Word> words;
  auto flush = [&]() {
    if (words.empty())
      return;
    r.packets.insert(words);
    NativeSceneryBatches::Entry entry{};
    entry.words = words.size();
    std::copy(words.begin(), words.end(), entry.commands);
    for (auto w : words)
      for (auto t : triangles(w.w0, w.w1)) {
        if (t[0] >= 128 || t[1] >= 128 || t[2] >= 128 || !available[t[0]] ||
            !available[t[1]] || !available[t[2]]) {
          words.clear();
          return;
        }
        entry.triangles[entry.count++] = {(uint8_t)t[0], (uint8_t)t[1],
                                          (uint8_t)t[2]};
      }
    words.clear();
    Bytes key;
    append(key, entry.triangles, entry.count * 3);
    append(key, loaded, sizeof loaded);
    if (seen.find(key) != seen.end())
      return;
    NativeMeshLod::Entry mesh{};
    mesh.count[0] = entry.count;
    memcpy(mesh.triangles[0], entry.triangles, entry.count * 3);
    memcpy(mesh.vertices, loaded, sizeof loaded);
    mesh.count[1] =
        NativeMeshLod::simplify(mesh.triangles[0], mesh.count[0], loaded,
                                mesh.triangles[1], 1, nullptr, true);
    mesh.count[2] =
        NativeMeshLod::simplify(mesh.triangles[1], mesh.count[1], loaded,
                                mesh.triangles[2], 2, nullptr, true);
    mesh.count[3] =
        NativeMeshLod::simplify(mesh.triangles[2], mesh.count[2], loaded,
                                mesh.triangles[3], 3, nullptr, true);
    if (mesh.count[1] == entry.count && mesh.count[2] == entry.count &&
        mesh.count[3] == entry.count)
      return;
    // Non-reducible runs produce no cache entry. Retaining their full vertex
    // keys across every level consumed tens of MiB during first launch.
    seen.insert(std::move(key));
    unsigned h = 0x9e3779b9;
    bool used[128]{};
    for (unsigned i = 0; i < entry.count; i++)
      for (unsigned id :
           {entry.triangles[i].a, entry.triangles[i].b, entry.triangles[i].c}) {
        h = (h ^ id) * 16777619u;
        used[id] = true;
      }
    for (unsigned i = 0; i < 128; i++)
      if (used[i])
        for (unsigned k = 0; k < 4; k++)
          h = (h ^ le32((const uint8_t *)(loaded + i) + 4 * k)) * 16777619u;
    mesh.hash = h ^ (h >> 16);
    for (unsigned tier = 0; tier < 4; tier++)
      std::fill(mesh.triangles[tier] + mesh.count[tier],
                mesh.triangles[tier] + 128, NativeMeshLod::Triangle{});
    r.bg.push_back(mesh);
  };
  for (unsigned at = 0; range(dl.size(), at, 8); at += 8) {
    unsigned x = be32(dl.data() + at), y = be32(dl.data() + at + 4),
             op = x >> 24;
    if (op == 0xbf || op == 0xb1) {
      if (words.size() == 32)
        flush();
      words.push_back({x, y});
      continue;
    }
    flush();
    if (op == 0xb8)
      break;
    if (op == 4) {
      unsigned n = (x & 65535) / 16, dest = x >> 16 & 15, offset = y & 0xffffff;
      if (y >> 24 != 14 || dest + n > 128 ||
          !range(vertices.size(), offset, n * 16)) {
        std::fill_n(available, 128, false);
        continue;
      }
      for (unsigned i = 0; i < n; i++) {
        Bytes v(vertices.begin() + offset + 16 * i,
                vertices.begin() + offset + 16 * i + 16);
        native_vertices(v);
        memcpy(loaded + dest + i, v.data(), 16);
        available[dest + i] = true;
      }
    } else if (op == 0xb2)
      std::fill_n(available, 128, false);
  }
  flush();
}
bool generate_meshes(const RomReader &rom, Results &results,
                     NativeConvertProgress cb) {
  std::vector<Job> jobs;
  std::map<std::pair<unsigned, unsigned>, unsigned> keys;
  unsigned n = sizeof model_layouts / sizeof *model_layouts;
  for (unsigned i = 0; i < n; i++) {
    if (!notify(cb, "Reading object meshes", i, n))
      return false;
    const auto &m = model_layouts[i];
    Bytes packed;
    if (!rom.read(m.offset, m.packed, packed))
      return false;
    Bytes raw = inflate_blob(packed.data(), packed.size());
    if (raw.empty())
      return false;
    auto batch = model_jobs(raw, m);
    for (auto &job : batch) {
      keys[{job.hash, job.vertices.size() / 16}]++;
      jobs.push_back(std::move(job));
    }
  }
  auto *work = new (std::nothrow) Workspace;
  if (!work)
    return false;
  for (unsigned i = 0; i < jobs.size(); i++) {
    if (!notify(cb, "Building object LODs", i, jobs.size())) {
      delete work;
      return false;
    }
    auto &job = jobs[i];
    if (keys[{job.hash, job.vertices.size() / 16}] != 1)
      continue;
    Coordinates out;
    if (reduce_job(job, *work, out))
      results.objects.push_back(std::move(out));
  }
  jobs.clear();
  jobs.shrink_to_fit();
  std::set<Bytes> seen;
  unsigned total_rooms=0,completed_rooms=0;
  for(const auto &layout:bg_layouts)total_rooms+=layout.rooms-1;
  for(const auto &layout:bg_layouts) {
    Bytes bg;
    if (!rom.read(layout.offset, layout.bytes, bg)) {
      delete work;
      return false;
    }
    unsigned table = be32(bg.data() + 4) & 0xffffff;
    for (unsigned room = 1; room < layout.rooms; room++) {
      if (!notify(cb, "Building room LODs and draw lists", completed_rooms++, total_rooms)) {
        delete work;
        return false;
      }
      unsigned at = table + room * 24;
      if (!range(bg.size(), at, 12)) {
        delete work;
        return false;
      }
      unsigned vp = be32(bg.data() + at) & 0xffffff;
      if (!vp)
        continue;
      if (vp >= bg.size()) {
        delete work;
        return false;
      }
      Bytes vertices = inflate_blob(bg.data() + vp, bg.size() - vp);
      if (vertices.empty()) {
        delete work;
        return false;
      }
      for (unsigned pass = 0; pass < 2; pass++) {
        unsigned dp = be32(bg.data() + at + 4 + 4 * pass) & 0xffffff;
        if (!dp)
          continue;
        if (dp >= bg.size()) {
          delete work;
          return false;
        }
        Bytes dl = inflate_blob(bg.data() + dp, bg.size() - dp);
        if (dl.empty()) {
          delete work;
          return false;
        }
        if (!pass) {
          auto job = room_job(room, vertices, dl);
          Coordinates out;
          if (!job.tri.empty() && reduce_job(job, *work, out))
            results.rooms.push_back(std::move(out));
        }
        bg_runs(results, vertices, dl, seen);
      }
    }
  }
  delete work;
  if (setup_cancelled)
    return false;
  std::stable_sort(
      results.bg.begin(), results.bg.end(),
      [](const auto &a, const auto &b) { return a.hash < b.hash; });
  return true;
}
bool generate_backdrop(const RomReader &rom, Bytes &out) {
  Bytes source;
  if (menu_bytes < 10 || !rom.read(menu_offset, menu_bytes, source))
    return false;
  auto *p = source.data();
  unsigned w = be16(p), h = be16(p + 2);
  if (w != 440 || h != 299)
    return false;
  Bytes pixels;
  pixels.reserve(w * h);
  for (unsigned at = 10; at + 1 < menu_bytes && pixels.size() < w * h;
       at += 2) {
    unsigned n = p[at];
    if (!n || pixels.size() + n > w * h)
      return false;
    pixels.insert(pixels.end(), n, p[at + 1]);
  }
  if (pixels.size() != w * h)
    return false;
  out.resize(320 * 240);
  for (unsigned y = 0; y < 240; y++) {
    unsigned sy = y * h / 240;
    for (unsigned x = 0; x < 320; x++)
      out[y * 320 + x] =
          (uint8_t)std::nearbyint((20 + 30 * double(sy) / (h - 1)) * .92 +
                                  pixels[sy * w + x * w / 320] * .08);
  }
  return true;
}
struct RenderOutput {
  FILE *file;
  unsigned bytes = 0, hash = 2166136261u;
  bool ok = true;
};
void append(RenderOutput &out, const void *data, size_t size) {
  if (!out.ok) return;
  if (size > max_payload - out.bytes ||
      fwrite(data, 1, size, out.file) != size) {
    out.ok = false;
    return;
  }
  out.hash = fnv(data, size, out.hash);
  out.bytes += size;
}
void word(RenderOutput &out, unsigned value) { append(out, &value, 4); }
void align4(RenderOutput &out) {
  const uint8_t zero[3]{};
  append(out, zero, (4 - out.bytes % 4) % 4);
}
// Emit the same immutable payload directly to the temporary cache. A second
// full-size vector exceeded the first-launch memory budget as stages grew.
bool serialize(const Results &r, RenderOutput &b) {
  size_t size = 0;
  for (const auto &o : r.objects) size += 8 + size_t(o.count) * 18;
  for (const auto &o : r.rooms) size += 12 + size_t(o.count) * 18;
  size = (size + 3) & ~size_t(3);
  size += r.bg.size() * sizeof(NativeMeshLod::Entry) +
          r.packets.size() * sizeof(NativeSceneryBatches::Entry) +
          r.title.size() + r.backdrop.size();
  if (size > max_payload) return false;
  for (const auto &o : r.objects) {
    word(b, o.hash);
    word(b, o.count);
    for (const auto &v : o.xyz)
      append(b, v.data(), v.size() * 2);
  }
  for (const auto &o : r.rooms) {
    word(b, o.room);
    word(b, o.hash);
    word(b, o.count);
    for (const auto &v : o.xyz)
      append(b, v.data(), v.size() * 2);
  }
  align4(b);
  for (const auto &e : r.bg)
    append(b, &e, sizeof e);
  for (const auto &words : r.packets) {
    NativeSceneryBatches::Entry e{};
    e.words = words.size();
    std::copy(words.begin(), words.end(), e.commands);
    for (auto w : words)
      for (auto t : triangles(w.w0, w.w1))
        e.triangles[e.count++] = {(uint8_t)t[0], (uint8_t)t[1], (uint8_t)t[2]};
    append(b, &e, sizeof e);
  }
  append(b, r.title.data(), r.title.size());
  append(b, r.backdrop.data(), r.backdrop.size());
  return b.ok && b.bytes == size;
}
struct FileData {
  unsigned objects = 0, rooms = 0, bg = 0, packets = 0;
  Bytes payload;
};
bool filename(char *p, const char *dir) {
  int n = snprintf(p, 512, "%s/native-render.dat", dir);
  return n > 0 && n < 512;
}
bool read_file(const char *path, FileData &out) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  uint8_t header[32];
  bool ok = fread(header, 1, 32, f) == 32 && !memcmp(header, render_magic, 8);
  unsigned bytes = 0, hash = 0;
  if (ok) {
    bytes = le32(header + 24);
    hash = le32(header + 28);
    out.objects = le32(header + 8);
    out.rooms = le32(header + 12);
    out.bg = le32(header + 16);
    out.packets = le32(header + 20);
    ok = out.objects <= 10000 && out.rooms <= max_room_records && out.bg <= 10000 &&
         out.packets <= 20000 && bytes >= 76800 && bytes <= max_payload;
  }
  if (ok) {
    out.payload.resize(bytes);
    ok = fread(out.payload.data(), 1, bytes, f) == bytes && fgetc(f) == EOF &&
         !ferror(f) && fnv(out.payload.data(), bytes) == hash;
  }
  fclose(f);
  return ok;
}
// Validate every variable-size span before exposing any pointer to workers.
bool layout(const FileData &f, NativeBakedLodRecord *objects,
            NativeRoomLodRecord *rooms, uint8_t *base,
            NativeMeshLod::Entry **bg, NativeSceneryBatches::Entry **packets,
            uint8_t **backdrop) {
  size_t at = 0, size = f.payload.size();
  const auto *p = f.payload.data();
  for (unsigned i = 0; i < f.objects + f.rooms; i++) {
    bool room = i >= f.objects;
    unsigned head = room ? 12 : 8;
    if (!range(size, at, head))
      return false;
    unsigned r = room ? le32(p + at) : 0, h = le32(p + at + (room ? 4 : 0)),
             n = le32(p + at + (room ? 8 : 4));
    at += head;
    if (!n || n > 65536 || (room && (!r || r >= NATIVE_ROOM_CAPACITY)) ||
        !range(size, at, size_t(n) * 18))
      return false;
    if (base) {
      const short *a = (const short *)(base + at);
      if (room)
        rooms[i - f.objects] = {r, h, n, a, a + n * 3, a + n * 6};
      else
        objects[i] = {h, n, a, a + n * 3, a + n * 6};
    }
    at += size_t(n) * 18;
  }
  at = (at + 3) & ~size_t(3);
  if (!range(size, at, size_t(f.bg) * sizeof(NativeMeshLod::Entry)))
    return false;
  const auto *meshes = (const NativeMeshLod::Entry *)(p + at);
  for (unsigned i = 0; i < f.bg; i++) {
    if (i && meshes[i - 1].hash > meshes[i].hash)
      return false;
    for (unsigned t = 0; t < 4; t++) {
      if (!meshes[i].count[t] || meshes[i].count[t] > 128 ||
          (t && meshes[i].count[t] > meshes[i].count[t - 1]))
        return false;
      for (unsigned k = 0; k < meshes[i].count[t]; k++) {
        auto tri = meshes[i].triangles[t][k];
        if (tri.a >= 128 || tri.b >= 128 || tri.c >= 128)
          return false;
      }
    }
  }
  if (base)
    *bg = (NativeMeshLod::Entry *)(base + at);
  at += size_t(f.bg) * sizeof(NativeMeshLod::Entry);
  if (!range(size, at, size_t(f.packets) * sizeof(NativeSceneryBatches::Entry)))
    return false;
  const auto *programs = (const NativeSceneryBatches::Entry *)(p + at);
  WordLess less;
  std::vector<NativeSceneryBatches::Word> previous;
  for (unsigned i = 0; i < f.packets; i++) {
    auto &e = programs[i];
    if (!e.words || e.words > 32 || !e.count || e.count > 128)
      return false;
    std::vector<NativeSceneryBatches::Word> words(e.commands,
                                                  e.commands + e.words);
    if (i && !less(previous, words))
      return false;
    previous = std::move(words);
    for (unsigned k = 0; k < e.words; k++)
      if (e.commands[k].w0 >> 24 != 0xbf && e.commands[k].w0 >> 24 != 0xb1)
        return false;
    for (unsigned k = 0; k < e.count; k++)
      if (e.triangles[k].a >= 128 || e.triangles[k].b >= 128 ||
          e.triangles[k].c >= 128)
        return false;
  }
  if (base)
    *packets = (NativeSceneryBatches::Entry *)(base + at);
  at += size_t(f.packets) * sizeof(NativeSceneryBatches::Entry);
  if (!range(size, at, 0x67f0 + 76800) || at + 0x67f0 + 76800 != size)
    return false;
  for (const auto &r : title_ranges)
    if (r.kind == 3)
      for (unsigned offset = 0; offset < r.bytes; offset += 8) {
        const auto *command = p + at + r.offset + offset;
        unsigned op = le32(command) >> 24, y = le32(command + 4);
        if ((op == 3 || op == 4 || op == 6 || op == 0xfd) && y >> 24 == 2) {
          unsigned address = y & 0xffffff;
          bool found = false;
          for (const auto &target : title_ranges)
            if (address >= target.offset &&
                address - target.offset < target.bytes) {
              found = true;
              break;
            }
          if (!found)
            return false;
        }
      }
  if (base)
    *backdrop = base + at + 0x67f0;
  return true;
}
} // namespace
extern "C" int native_derived_generate(const char *rom_path, const char *output,
                                       NativeConvertProgress cb) {
  setup_cancelled = false;
  struct ResetProgress {
    ~ResetProgress() { setup_progress = nullptr; }
  } reset;
  if (!notify(cb, "Preparing rendering data", 0, 1))
    return 0;
  RomReader rom;
  if (!rom.open(rom_path))
    return 0;
  // Entry is called only after native_convert's complete normalized SHA-1
  // check.
  Results result;
  if (!rom.read(0x29e560, 0x67f0, result.title)) return 0;
  for (const auto &r : title_ranges) {
    auto *p = result.title.data() + r.offset;
    if (r.kind == 2)
      for (unsigned i = 0; i < r.bytes; i += 16)
        for (unsigned j = 0; j < 12; j += 2)
          std::swap(p[i + j], p[i + j + 1]);
    else if (r.kind == 1 || r.kind == 3)
      for (unsigned i = 0; i < r.bytes; i += 4) {
        std::swap(p[i], p[i + 3]);
        std::swap(p[i + 1], p[i + 2]);
      }
  }
  if (!generate_backdrop(rom, result.backdrop) ||
      !generate_meshes(rom, result, cb))
    return 0;
  if (!notify(cb, "Saving rendering cache", 1, 1))
    return 0;
  FILE *f = fopen(output, "wb");
  if (!f) return 0;
  // Reserve the fixed header; publish its counts/hash only after every payload
  // write succeeds. native_convert commits this temporary file after close.
  const uint8_t empty_header[32]{};
  RenderOutput bytes{f};
  bool ok = fwrite(empty_header, 1, sizeof empty_header, f) == sizeof empty_header &&
            serialize(result, bytes);
  Bytes header;
  append(header, render_magic, 8);
  word(header, result.objects.size());
  word(header, result.rooms.size());
  word(header, result.bg.size());
  word(header, result.packets.size());
  word(header, bytes.bytes);
  word(header, bytes.hash);
  ok = ok && !fseek(f, 0, SEEK_SET) &&
       fwrite(header.data(), 1, header.size(), f) == header.size();
  if (fclose(f))
    ok = false;
  if (!ok)
    remove(output);
  return ok;
}
extern "C" int native_derived_valid(const char *dir) {
  char name[512];
  FileData data;
  return filename(name, dir) && read_file(name, data) &&
         layout(data, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}
extern "C" void native_derived_unmount(void) {
  free(mounted);
  free(native_baked_lods);
  free(native_room_lods);
  mounted = nullptr;
  native_baked_lods = nullptr;
  native_room_lods = nullptr;
  native_bg_lods = nullptr;
  native_room_packets = nullptr;
  original_bottom_background = nullptr;
  native_baked_lod_count = native_room_lod_count = native_bg_lod_count =
      native_room_packet_count = 0;
}
extern "C" int native_derived_mount(const char *dir) {
  if (mounted)
    return 0;
  char name[512];
  FileData f;
  if (!filename(name, dir) || !read_file(name, f) ||
      !layout(f, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
    return 0;
  auto *base = (uint8_t *)malloc(f.payload.size());
  auto *objects = (NativeBakedLodRecord *)calloc(f.objects ? f.objects : 1,
                                                 sizeof(NativeBakedLodRecord));
  auto *rooms = (NativeRoomLodRecord *)calloc(f.rooms ? f.rooms : 1,
                                              sizeof(NativeRoomLodRecord));
  if (!base || !objects || !rooms) {
    free(base);
    free(objects);
    free(rooms);
    return 0;
  }
  memcpy(base, f.payload.data(), f.payload.size());
  bool ok = layout(f, objects, rooms, base, &native_bg_lods,
                   &native_room_packets, &original_bottom_background);
  if (!ok) {
    free(base);
    free(objects);
    free(rooms);
    return 0;
  }
  const auto *title = original_bottom_background - 0x67f0;
  for (const auto &r : title_ranges)
    memcpy(r.destination, title + r.offset, r.bytes);
  for (const auto &r : title_ranges)
    if (r.kind == 3)
      for (unsigned at = 0; at < r.bytes; at += 8) {
        auto *p = r.destination + at;
        unsigned op = le32(p) >> 24, y = le32(p + 4);
        if ((op == 3 || op == 4 || op == 6 || op == 0xfd) && y >> 24 == 2) {
          unsigned offset = y & 0xffffff;
          bool found = false;
          for (const auto &target : title_ranges)
            if (offset >= target.offset &&
                offset - target.offset < target.bytes) {
              uintptr_t address =
                  (uintptr_t)(target.destination + offset - target.offset);
              unsigned value = (unsigned)address;
              memcpy(p + 4, &value, 4);
              found = true;
              break;
            }
          if (!found) {
            free(base);
            free(objects);
            free(rooms);
            native_bg_lods = nullptr;
            native_room_packets = nullptr;
            original_bottom_background = nullptr;
            return 0;
          }
        }
      }
  mounted = base;
  native_baked_lods = objects;
  native_room_lods = rooms;
  native_baked_lod_count = f.objects;
  native_room_lod_count = f.rooms;
  native_bg_lod_count = f.bg;
  native_room_packet_count = f.packets;
  return 1;
}
