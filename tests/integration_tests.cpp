#include <absim.hpp>

#include <cstdio>
#include <fstream>
#include <memory>

#define WRITE_IMAGES 0

#if WRITE_IMAGES
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

static std::unique_ptr<absim::arduboy_t> arduboy;

static int test(char const* name)
{
    int r = 0;

    std::string fname =
        std::string(TESTS_DIR "/") + name + "/" + name + ".ino-arduboy-fx.hex";
    std::ifstream f(fname);

    auto err = arduboy->load_file("test.hex", f);
    if(!err.empty()) return 1;
    arduboy->reset();
    auto const& d = arduboy->cpu.serial_bytes;
    for(int i = 0; i < 10000; ++i) // up to ten seconds
    {
        arduboy->advance(1'000'000'000ull); // 1 ms
        if(!d.empty()) break;
        arduboy->cpu.sound_buffer.clear();
    }

    bool pass = (d.size() == 1 && d[0] == 'P');

    printf("   %-30s : %s\n", name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static void advance(int ms)
{
    for(int i = 0; i < ms; ++i)
    {
        arduboy->advance(1'000'000'000ull); // 1 ms
        arduboy->cpu.sound_buffer.clear();
    }
}

static int compare_image(char const* dir, int n)
{
    int r = 0;
    char ifname[256];
    snprintf(ifname, sizeof(ifname), "%s/%s/image%d.bin", TESTS_DIR, dir, n);
#if WRITE_IMAGES
    std::ofstream fi(ifname, std::ios::binary);
    fi.write((char const*)arduboy->display.filtered_pixels.data(), 8192);
    snprintf(ifname, sizeof(ifname), "%s/%s/image%d.png", TESTS_DIR, dir, n);
    stbi_write_png(ifname, 128, 64, 1, arduboy->display.filtered_pixels.data(), 128);
#else
    std::ifstream fi(ifname, std::ios::binary);
    std::vector<char> id;
    id.resize(8192);
    fi.read(id.data(), 8192);
    for(size_t i = 0; i < 8192; ++i)
    {
        bool w0 = (uint8_t)arduboy->display.filtered_pixels[i] < 128;
        bool w1 = (uint8_t)id[i] < 128;
        if(w0 != w1)
            r = 1;
    }
#endif
    return r;
}

static int image_test(char const* dir, char const* game)
{
    std::ifstream f(std::string(TESTS_DIR "/") + dir + "/" + game, std::ios::binary);
    auto err = arduboy->load_file(game, f);
    int r = 0;
    if(!err.empty()) r = 1;
    arduboy->cfg.display_type = absim::display_t::type_t::SSD1306;
    arduboy->cfg.fxport_reg = 0x2b;
    arduboy->cfg.fxport_mask = 1 << 1;
    arduboy->cfg.bootloader = true;
    arduboy->cfg.boot_to_menu = false;
    arduboy->reset();

    int n = 0;

    arduboy->cpu.data[0x23] = 0x10;
    arduboy->cpu.data[0x2c] = 0x40;
    arduboy->cpu.data[0x2f] = 0xf0;
    advance(1000);
    r |= compare_image(dir, n++);
    for(int i = 0; i < 9; ++i)
    {
        advance(1000);
        arduboy->cpu.data[0x2c] = 0x00;
        if(i != 0)
            arduboy->cpu.data[0x2f] = 0xa0;
        advance(100);
        arduboy->cpu.data[0x2c] = 0x40;
        arduboy->cpu.data[0x2f] = 0xf0;
        advance(1000);
        r |= compare_image(dir, n++);
    }

    printf("   %-30s : %s\n", dir, r ? "FAIL" : "PASS");

    return r;
}

int main()
{
    int r = 0;
    arduboy = std::make_unique<absim::arduboy_t>();
    arduboy->display.enable_filter = true;

    printf("Integration tests...\n");
    r |= test("float");
    r |= test("instructions");
    r |= test("signature");
    r |= test("timer_tcnt_write");

    printf("\nImage tests...\n");
    r |= image_test("arduchess", "arduchess.hex");
    r |= image_test("ardugolf", "ardugolf.hex");
    r |= image_test("ardugolf_fx", "ardugolf_fx.arduboy");
    r |= image_test("dazzledash", "dazzledash.arduboy");
    r |= image_test("summercamp", "summercamp.arduboy");

    return r;
}
