#include "GlobeRenderer.h"
#include <cmath>

namespace wb::ui {

namespace {
constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;

float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
float smoothstep(float a, float b, float x) {
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
float mixf(float a, float b, float t) { return a + (b - a) * t; }

// 3D value noise on the sphere (no seam at the date line, no pinch at the poles).
juce::uint32 hash3(int x, int y, int z, juce::uint32 seed) {
    juce::uint32 h = seed * 0x9E3779B1u ^ juce::uint32(x) * 0x85EBCA77u ^ juce::uint32(y) * 0xC2B2AE3Du ^ juce::uint32(z) * 0x27D4EB2Fu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    return h ^ (h >> 15);
}
float lattice(int x, int y, int z, juce::uint32 seed) { return float(hash3(x, y, z, seed) & 0xffffu) / 65535.0f; }
float valueNoise(float x, float y, float z, juce::uint32 seed) {
    const int xi = int(std::floor(x)), yi = int(std::floor(y)), zi = int(std::floor(z));
    float fx = x - float(xi), fy = y - float(yi), fz = z - float(zi);
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    fz = fz * fz * (3 - 2 * fz);
    const auto at = [&](int dx, int dy, int dz) { return lattice(xi + dx, yi + dy, zi + dz, seed); };
    const float x00 = mixf(at(0, 0, 0), at(1, 0, 0), fx), x10 = mixf(at(0, 1, 0), at(1, 1, 0), fx);
    const float x01 = mixf(at(0, 0, 1), at(1, 0, 1), fx), x11 = mixf(at(0, 1, 1), at(1, 1, 1), fx);
    return mixf(mixf(x00, x10, fy), mixf(x01, x11, fy), fz);
}
struct Vec { float x, y, z; };
float fbm(Vec p, float scale, int octaves, juce::uint32 seed) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * valueNoise(p.x * scale + 17.0f, p.y * scale + 31.0f, p.z * scale + 7.0f, seed + juce::uint32(o) * 101u);
        norm += amp;
        scale *= 2.03f;
        amp *= 0.5f;
    }
    return sum / norm;
}
// Ridges: 1 on the noise's mid line, falling away on both sides (cracks, dune crests).
float ridge(Vec p, float scale, int octaves, juce::uint32 seed) { return 1.0f - std::abs(fbm(p, scale, octaves, seed) * 2.0f - 1.0f); }

struct Rgb {
    float r, g, b;
};
Rgb rgb(juce::uint32 c) { return { float((c >> 16) & 255) / 255.0f, float((c >> 8) & 255) / 255.0f, float(c & 255) / 255.0f }; }
Rgb mix(Rgb a, Rgb b, float t) { return { mixf(a.r, b.r, t), mixf(a.g, b.g, t), mixf(a.b, b.b, t) }; }
Rgb scale(Rgb a, float k) { return { a.r * k, a.g * k, a.b * k }; }
juce::uint32 pack(Rgb c) {
    const auto q = [](float v) { return juce::uint32(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return (q(c.r) << 16) | (q(c.g) << 8) | q(c.b);
}
juce::uint8 byte(float v) { return juce::uint8(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

// A few impact craters for the rocky world: centres and sizes on the sphere.
struct Crater {
    Vec c;
    float radius;
};
std::vector<Crater> craters(int count, juce::uint32 seed) {
    std::vector<Crater> out;
    for (int i = 0; i < count; ++i) {
        const float u = lattice(i, 1, 0, seed), v = lattice(i, 2, 0, seed), s = lattice(i, 3, 0, seed);
        const float lon = u * twoPi, lat = std::asin(v * 2.0f - 1.0f);
        out.push_back({ { std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon) }, 0.03f + 0.13f * s * s });
    }
    return out;
}
} // namespace

juce::Colour GlobeRenderer::atmosphere(int planet) {
    switch (planet) {
        case 0: return juce::Colour(0xffb8b0a4);   // Rocky: a thin, pale haze
        case 1: return juce::Colour(0xff5fb4ff);   // Ocean
        case 2: return juce::Colour(0xff9ff2ff);   // Ice Giant
        case 3: return juce::Colour(0xffffc68a);   // Gas Giant
        case 4: return juce::Colour(0xffffb86a);   // Desert World: dust
        case 5: return juce::Colour(0xffff6a2a);   // Lava World
        case 6: return juce::Colour(0xff8fe0a8);   // Forest World
        default: return juce::Colour(0xff5fd4e6);
    }
}

bool GlobeRenderer::hasRings(int planet) { return planet == 2 || planet == 3; }

const GlobeRenderer::Maps& GlobeRenderer::maps(int planet) {
    planet = std::clamp(planet, 0, planetCount - 1);
    auto& slot = cache[std::size_t(planet)];
    if (slot != nullptr) return *slot;
    auto m = std::make_unique<Maps>();
    constexpr int W = Maps::width, H = Maps::height;
    m->surface.assign(W * H, 0);
    m->cloud.assign(W * H, 0);
    m->lights.assign(W * H, 0);
    m->glow.assign(W * H, 0);
    m->shine.assign(W * H, 0);
    const auto pits = planet == 0 ? craters(70, 404) : std::vector<Crater> {};
    const juce::uint32 seed = 1000u + juce::uint32(planet) * 77u;
    m->cloudCover = std::array<float, planetCount> { 0.08f, 0.5f, 0.0f, 0.0f, 0.18f, 0.3f, 0.42f }[std::size_t(planet)];
    m->cloudColour = std::array<juce::Colour, planetCount> { juce::Colour(0xffd8d2c8), juce::Colour(0xfff6f8fc), juce::Colour(0xffffffff),
                                                            juce::Colour(0xfff8ead0), juce::Colour(0xffe8c48e), juce::Colour(0xff4a3c38),
                                                            juce::Colour(0xfff2f6f2) }[std::size_t(planet)];
    for (int j = 0; j < H; ++j) {
        const float lat = (0.5f - (float(j) + 0.5f) / float(H)) * pi;
        for (int i = 0; i < W; ++i) {
            const float lon = ((float(i) + 0.5f) / float(W) - 0.5f) * twoPi;
            const Vec p { std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon) };
            const int k = j * W + i;
            Rgb c {};
            float cloud = fbm(p, 2.6f, 5, seed + 9u);
            // Clouds stretch along the latitudes, like weather bands.
            cloud = 0.65f * cloud + 0.35f * fbm({ p.x * 0.6f, p.y * 2.2f, p.z * 0.6f }, 4.0f, 3, seed + 19u);
            float lights = 0, glow = 0, shine = 0;
            switch (planet) {
                case 0: {   // Rocky: grey-brown plains and highlands, craters
                    const float h = fbm(p, 2.4f, 6, seed), d = fbm(p, 11.0f, 3, seed + 3u);
                    c = mix(rgb(0x5b4f45), rgb(0xa3968a), smoothstep(0.3f, 0.75f, h));
                    c = mix(c, rgb(0x7a6a58), 0.35f * smoothstep(0.45f, 0.7f, fbm(p, 5.0f, 3, seed + 5u)));
                    c = scale(c, 0.82f + 0.36f * d);
                    for (const auto& pit : pits) {
                        const float dot = p.x * pit.c.x + p.y * pit.c.y + p.z * pit.c.z;
                        const float dist = std::acos(std::clamp(dot, -1.0f, 1.0f)) / pit.radius;
                        if (dist > 1.25f) continue;
                        if (dist < 1.0f) c = scale(c, 0.72f + 0.2f * dist * dist);
                        else c = scale(c, 1.0f + 0.22f * (1.0f - std::abs(dist - 1.1f) / 0.15f));
                    }
                    lights = smoothstep(0.55f, 0.75f, fbm(p, 7.0f, 3, seed + 41u)) * lattice(i, j, 3, seed);
                    break;
                }
                case 1: {   // Ocean: deep blue, a few islands
                    const float h = fbm(p, 2.2f, 6, seed);
                    const float land = smoothstep(0.6f, 0.63f, h);
                    const Rgb deep = mix(rgb(0x082a57), rgb(0x0f4a85), smoothstep(0.3f, 0.6f, h));
                    const Rgb shore = mix(rgb(0x2a8fb8), rgb(0x46b9c8), smoothstep(0.56f, 0.6f, h));
                    const Rgb water = mix(deep, shore, smoothstep(0.5f, 0.6f, h));
                    const Rgb ground = mix(rgb(0xd8c48a), rgb(0x3f8a4a), smoothstep(0.63f, 0.67f, h));
                    c = mix(water, ground, land);
                    shine = 1.0f - land;
                    lights = land * smoothstep(0.45f, 0.7f, fbm(p, 9.0f, 2, seed + 41u)) * lattice(i, j, 3, seed);
                    break;
                }
                case 2: {   // Ice Giant: pale cyan bands of ice
                    const float warp = fbm(p, 3.0f, 4, seed);
                    const float band = std::sin(lat * 11.0f + warp * 3.0f) * 0.5f + 0.5f;
                    const float fine = fbm({ p.x, p.y * 6.0f, p.z }, 3.5f, 3, seed + 2u);
                    c = mix(rgb(0x3a86a8), rgb(0xd2f4f8), smoothstep(0.1f, 0.9f, band));
                    c = mix(c, rgb(0x2a6a8c), 0.5f * smoothstep(0.55f, 0.8f, fine));
                    c = mix(c, rgb(0xf2fcff), 0.5f * smoothstep(1.15f, 1.45f, std::abs(lat)));
                    shine = 0.35f;
                    break;
                }
                case 3: {   // Gas Giant: swirling bands and a great storm
                    const float warp = fbm(p, 2.5f, 5, seed) - 0.5f;
                    const float swirl = fbm({ p.x * 1.4f, p.y * 5.0f, p.z * 1.4f }, 3.0f, 4, seed + 4u);
                    const float y = lat + 0.22f * warp + 0.05f * std::sin(lon * 3.0f + lat * 6.0f);
                    const float band = std::sin(y * 15.0f + swirl * 2.5f) * 0.5f + 0.5f;
                    const float band2 = std::sin(y * 6.0f + 1.0f) * 0.5f + 0.5f;
                    c = mix(rgb(0x7a4426), rgb(0xf4e0b8), smoothstep(0.2f, 0.8f, band));
                    c = mix(c, rgb(0xd09a5a), 0.5f * smoothstep(0.3f, 0.7f, band2) * band);
                    c = mix(c, rgb(0x5a2c1a), 0.55f * smoothstep(0.65f, 0.95f, band2) * (1.0f - band));
                    c = scale(c, 0.9f + 0.2f * swirl);
                    // The great storm: an oval vortex south of the equator.
                    const float sx = std::remainder(lon - 1.1f, twoPi) / 0.42f, sy = (lat + 0.38f) / 0.17f;
                    const float storm = sx * sx + sy * sy;
                    if (storm < 1.6f) {
                        const float a = std::atan2(sy, sx), r = std::sqrt(storm);
                        const float spiral = std::sin(a * 2.0f + r * 9.0f) * 0.5f + 0.5f;
                        const Rgb eye = mix(rgb(0xc8582e), rgb(0xe8a070), spiral * r);
                        c = mix(c, eye, smoothstep(1.6f, 0.6f, storm));
                    }
                    lights = 0.4f * smoothstep(0.82f, 0.9f, lattice(i, j, 5, seed)) * smoothstep(0.7f, 0.3f, std::abs(lat));
                    break;
                }
                case 4: {   // Desert World: dunes, rock and salt flats
                    const float h = fbm(p, 2.0f, 5, seed);
                    const float dunes = ridge({ p.x * 2.2f, p.y * 0.8f, p.z * 2.2f }, 9.0f, 2, seed + 6u);
                    c = mix(rgb(0xb8743c), rgb(0xe8be7a), smoothstep(0.35f, 0.7f, h));
                    c = mix(c, rgb(0xf4d8a0), 0.45f * smoothstep(0.7f, 0.95f, dunes));
                    c = mix(c, rgb(0x6a4a34), 0.6f * smoothstep(0.66f, 0.76f, fbm(p, 4.5f, 4, seed + 8u)));
                    c = mix(c, rgb(0xf0e6d4), 0.7f * smoothstep(0.2f, 0.12f, h));
                    lights = smoothstep(0.6f, 0.78f, fbm(p, 8.0f, 2, seed + 41u)) * lattice(i, j, 3, seed);
                    break;
                }
                case 5: {   // Lava World: dark basalt cracked with fire
                    const float h = fbm(p, 2.6f, 5, seed);
                    c = mix(rgb(0x1e1614), rgb(0x4a3a32), smoothstep(0.3f, 0.75f, h));
                    c = scale(c, 0.85f + 0.3f * fbm(p, 12.0f, 2, seed + 2u));
                    const float crack = ridge(p, 3.2f, 4, seed + 10u);
                    const float fine = ridge(p, 9.0f, 2, seed + 12u);
                    glow = std::max(smoothstep(0.86f, 0.97f, crack), 0.6f * smoothstep(0.9f, 0.98f, fine));
                    glow = std::max(glow, smoothstep(0.72f, 0.8f, fbm(p, 2.0f, 3, seed + 14u)));   // lava lakes
                    c = mix(c, rgb(0x5a1a0c), glow * 0.8f);
                    lights = 0.5f * smoothstep(0.62f, 0.8f, fbm(p, 8.0f, 2, seed + 41u)) * lattice(i, j, 3, seed);
                    break;
                }
                default: {   // Forest World: green continents, blue seas, white poles
                    const float h = fbm(p, 2.0f, 6, seed);
                    const float land = smoothstep(0.49f, 0.52f, h);
                    const Rgb water = mix(rgb(0x0b3a66), rgb(0x2a7aa0), smoothstep(0.38f, 0.5f, h));
                    const float wood = fbm(p, 8.0f, 3, seed + 3u);
                    Rgb ground = mix(rgb(0x1f5a2a), rgb(0x4e8c3a), smoothstep(0.35f, 0.7f, wood));
                    ground = mix(ground, rgb(0x6a6a48), smoothstep(0.66f, 0.74f, h));
                    c = mix(water, ground, land);
                    const float pole = smoothstep(1.18f, 1.32f, std::abs(lat) + 0.12f * (fbm(p, 6.0f, 2, seed + 5u) - 0.5f));
                    c = mix(c, rgb(0xf0f6f8), pole);
                    shine = (1.0f - land) * (1.0f - pole);
                    lights = land * (1.0f - pole) * smoothstep(0.55f, 0.75f, fbm(p, 9.0f, 2, seed + 41u)) * lattice(i, j, 3, seed);
                    break;
                }
            }
            m->surface[std::size_t(k)] = pack(c);
            m->cloud[std::size_t(k)] = byte(cloud);
            m->lights[std::size_t(k)] = byte(lights > 0.35f ? lights : 0.0f);
            m->glow[std::size_t(k)] = byte(glow);
            m->shine[std::size_t(k)] = byte(shine);
        }
    }
    slot = std::move(m);
    return *slot;
}

void GlobeRenderer::buildTable(int diameter) {
    tableSize = diameter;
    table.clear();
    frame = juce::Image(juce::Image::ARGB, diameter, diameter, true);
    accum.assign(std::size_t(diameter * diameter * 4), 0.0f);
    const float radius = float(diameter) * 0.5f;
    // The light comes from the upper left, in front.
    const float lx = -0.55f, ly = -0.48f, lz = 0.68f, ln = std::sqrt(lx * lx + ly * ly + lz * lz);
    const float hx = lx / ln, hy = ly / ln, hz = lz / ln + 1.0f, hn = std::sqrt(hx * hx + hy * hy + hz * hz);
    const float ct = std::cos(tilt), st = std::sin(tilt);
    for (int y = 0; y < diameter; ++y)
        for (int x = 0; x < diameter; ++x) {
            const float nx = (float(x) + 0.5f - radius) / radius, ny = (float(y) + 0.5f - radius) / radius;
            const float d2 = nx * nx + ny * ny;
            const float edge = clamp01(radius - std::sqrt(d2) * radius + 0.5f);
            if (edge <= 0.0f) continue;
            const float nz = std::sqrt(std::max(0.0f, 1.0f - d2));
            const float diffuse = (nx * lx + ny * ly + nz * lz) / ln;
            Pixel px {};
            px.offset = y * diameter + x;
            px.light = smoothstep(-0.12f, 0.22f, diffuse) * (0.3f + 0.7f * clamp01(diffuse));
            px.rim = std::pow(1.0f - nz, 2.2f);
            px.spec = std::pow(std::max(0.0f, (nx * hx + ny * hy + nz * hz) / hn), 38.0f);
            px.edge = edge;
            // The planet's axis leans: rotate into its frame for latitude and longitude.
            const float tx = nx * ct - ny * st, ty = nx * st + ny * ct;
            const float lat = std::asin(std::clamp(-ty, -1.0f, 1.0f));
            const float lon = std::atan2(tx, std::max(nz, 1.0e-4f));
            px.row = std::clamp(int((0.5f - lat / pi) * float(Maps::height)), 0, Maps::height - 1);
            px.u = (lon / twoPi + 0.5f) * float(Maps::width);
            table.push_back(px);
        }
}

void GlobeRenderer::renderPlanet(int planet, const GlobeLook& look, float weight, bool first) {
    const auto& m = maps(planet);
    constexpr int W = Maps::width;
    const float shift = -look.spin / twoPi * float(W);
    // Clouds drift a little faster than the ground.
    const float cloudShift = shift * 1.18f - look.time * 2.0f;
    const float cover = clamp01(m.cloudCover + look.clouds * (1.0f - m.cloudCover) * (planet == 2 || planet == 3 ? 0.25f : 0.7f));
    const float cloudFrom = 1.0f - cover, cloudTo = cloudFrom + 0.28f;
    const Rgb cloudColour { m.cloudColour.getFloatRed(), m.cloudColour.getFloatGreen(), m.cloudColour.getFloatBlue() };
    const auto atm = atmosphere(planet).interpolatedWith(look.accent, 0.25f);
    const Rgb air { atm.getFloatRed(), atm.getFloatGreen(), atm.getFloatBlue() };
    const Rgb city = rgb(0xffd890), fire = rgb(0xff6a1c), fireHot = rgb(0xffd060);
    const float lightsAmount = look.lights, heat = 0.6f + 0.4f * look.heat;
    const auto wrap = [](float u) {
        u = std::fmod(u, float(W));
        return u < 0.0f ? u + float(W) : u;
    };
    if (first) std::fill(accum.begin(), accum.end(), 0.0f);
    for (const auto& px : table) {
        const float u = wrap(px.u + shift);
        const int i0 = int(u), i1 = (i0 + 1) % W;
        const float f = u - float(i0);
        const int row = px.row * W;
        const auto unpack = [](juce::uint32 c) { return Rgb { float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255) }; };
        const Rgb a = unpack(m.surface[std::size_t(row + i0)]), b = unpack(m.surface[std::size_t(row + i1)]);
        Rgb c = scale(mix(a, b, f), 1.0f / 255.0f);
        const float light = px.light;
        c = scale(c, 0.05f + 1.05f * light);
        c.r += px.spec * float(m.shine[std::size_t(row + i0)]) / 255.0f * 0.55f * light;
        c.g += px.spec * float(m.shine[std::size_t(row + i0)]) / 255.0f * 0.6f * light;
        c.b += px.spec * float(m.shine[std::size_t(row + i0)]) / 255.0f * 0.65f * light;
        // Clouds, lit like the ground, casting a faint shade.
        const int ci = int(wrap(px.u + cloudShift));
        const float cloud = smoothstep(cloudFrom, cloudTo, float(m.cloud[std::size_t(row + ci)]) / 255.0f) * 0.92f;
        // The night side: city lights and glowing cracks (dimmed under clouds).
        const float night = (1.0f - light) * (1.0f - light);
        const float lit = float(m.lights[std::size_t(row + i0)]) / 255.0f * lightsAmount * night * (1.0f - 0.7f * cloud);
        const float glow = float(m.glow[std::size_t(row + i0)]) / 255.0f * heat * (1.0f - 0.5f * cloud);
        c = mix(c, scale(cloudColour, 0.06f + 1.0f * light), cloud);
        c.r += lit * city.r * 1.2f + glow * mixf(fire.r, fireHot.r, glow) * (0.55f + 0.45f * night);
        c.g += lit * city.g * 1.2f + glow * mixf(fire.g, fireHot.g, glow) * (0.55f + 0.45f * night);
        c.b += lit * city.b * 1.2f + glow * mixf(fire.b, fireHot.b, glow) * (0.55f + 0.45f * night);
        // The atmosphere: brightest at the lit limb.
        const float haze = px.rim * (0.2f + 0.8f * light) * (planet == 0 ? 0.45f : planet == 2 || planet == 3 ? 0.55f : 0.95f);
        c.r += air.r * haze;
        c.g += air.g * haze;
        c.b += air.b * haze;
        // The world's grade: cold blues, hot ambers, a darker red for danger.
        if (look.cold > 0.0f) c = mix(c, { c.r * 0.8f, c.g * 0.95f, c.b * 1.15f + 0.04f }, look.cold);
        if (look.warm > 0.0f) c = mix(c, { c.r * 1.12f + 0.03f, c.g * 0.95f, c.b * 0.78f }, look.warm);
        if (look.danger > 0.0f) c = mix(c, { c.r * 0.75f + 0.05f, c.g * 0.55f, c.b * 0.55f }, look.danger);
        // Power: the planet dims rather than fading out (the rings never show through it).
        c = scale(c, look.alpha);
        const float alpha = px.edge * weight;
        auto* out = accum.data() + std::size_t(px.offset) * 4;
        out[0] += c.r * alpha;
        out[1] += c.g * alpha;
        out[2] += c.b * alpha;
        out[3] += alpha;
    }
}

void GlobeRenderer::drawRings(juce::Graphics& g, juce::Rectangle<float> area, int planet, float alpha, bool front) const {
    if (alpha <= 0.01f || !hasRings(planet)) return;
    const auto c = area.getCentre();
    const float r = area.getWidth() * 0.5f;
    const bool ice = planet == 2;
    const float inner = ice ? 1.32f : 1.28f, outer = ice ? 1.72f : 1.9f, flat = 0.22f;
    const int bands = ice ? 9 : 16;
    const auto colourA = ice ? juce::Colour(0xffd8f6ff) : juce::Colour(0xfff0dcb0);
    const auto colourB = ice ? juce::Colour(0xff7fc8dc) : juce::Colour(0xffb08058);
    // Front: the near half (below the planet's centre in the ring's own frame).
    const float from = front ? 0.0f : pi, to = front ? pi : twoPi;
    const float bandWidth = (outer - inner) * r / float(bands);
    for (int b = 0; b < bands; ++b) {
        const float t = (float(b) + 0.5f) / float(bands);
        const float radius = (inner + (outer - inner) * t) * r;
        // Uneven bands with a gap (like a Cassini division) two thirds of the way out.
        const float density = (0.35f + 0.65f * lattice(b, 7, 1, 99u)) * (std::abs(t - 0.68f) < 0.07f ? 0.12f : 1.0f);
        juce::Path arc;
        arc.addCentredArc(c.x, c.y, radius, radius * flat, tilt, from + pi * 0.5f, to + pi * 0.5f, true);
        g.setColour(colourA.interpolatedWith(colourB, lattice(b, 3, 2, 98u)).withAlpha(alpha * density * (front ? 0.62f : 0.5f)));
        g.strokePath(arc, juce::PathStrokeType(bandWidth * 0.92f));
    }
}

void GlobeRenderer::draw(juce::Graphics& g, juce::Rectangle<float> area, int planet, int previous, float blend, const GlobeLook& look) {
    const int diameter = std::max(8, juce::roundToInt(area.getWidth()));
    if (diameter != tableSize) buildTable(diameter);
    blend = clamp01(blend);
    const bool fading = previous != planet && previous >= 0 && blend < 0.999f;
    const auto centre = area.getCentre();
    const float r = area.getWidth() * 0.5f;

    // The glow of the atmosphere around the disc, stronger on the lit side.
    {
        auto atm = atmosphere(planet).interpolatedWith(look.accent, 0.3f);
        if (fading) atm = atmosphere(previous).interpolatedWith(atm, blend);
        const float thick = planet == 0 ? 0.08f : 0.16f;
        const auto glow = atm.withAlpha(0.42f * look.alpha);
        juce::ColourGradient halo(glow, centre.x - r * 0.18f, centre.y - r * 0.16f, glow.withAlpha(0.0f), centre.x - r * 0.18f + r * (1.0f + thick + 0.1f),
                                  centre.y - r * 0.16f, true);
        halo.addColour(r * 0.98f / (r * (1.0f + thick + 0.1f)), glow);
        g.setGradientFill(halo);
        g.fillEllipse(area.expanded(r * (thick + 0.12f)));
    }
    // Rings: the far half behind the planet.
    const float ringNow = hasRings(planet) ? (fading ? blend : 1.0f) : 0.0f;
    const float ringBefore = fading && hasRings(previous) ? 1.0f - blend : 0.0f;
    drawRings(g, area, planet, ringNow * look.alpha, false);
    if (fading) drawRings(g, area, previous, ringBefore * look.alpha, false);

    if (fading) renderPlanet(previous, look, 1.0f - blend, true);
    renderPlanet(planet, look, fading ? blend : 1.0f, !fading);
    {
        juce::Image::BitmapData data(frame, juce::Image::BitmapData::writeOnly);
        for (const auto& px : table) {
            const float* a = accum.data() + std::size_t(px.offset) * 4;
            const float alpha = std::min(1.0f, a[3]);
            const auto q = [alpha](float v) { return juce::uint8(std::clamp(v, 0.0f, alpha) * 255.0f + 0.5f); };
            auto* p = reinterpret_cast<juce::PixelARGB*>(data.getPixelPointer(px.offset % tableSize, px.offset / tableSize));
            p->setARGB(juce::uint8(alpha * 255.0f + 0.5f), q(a[0]), q(a[1]), q(a[2]));
        }
    }
    g.drawImage(frame, area, juce::RectanglePlacement::stretchToFit);

    // Rings: the near half in front.
    drawRings(g, area, planet, ringNow * look.alpha, true);
    if (fading) drawRings(g, area, previous, ringBefore * look.alpha, true);
}

} // namespace wb::ui
