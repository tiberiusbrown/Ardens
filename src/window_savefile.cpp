#include "imgui.h"

#include "common.hpp"

#include <filesystem>
#include <inttypes.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static char const* DELETE_POPUP = "Delete Save File";

void window_savefile(bool& open)
{
    using namespace ImGui;
    if(!open) return;

    bool open_confirm_delete_popup = false;

    SetNextWindowSize({ 300 * pixel_ratio, 100 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Save File", &open) && arduboy.cpu.decoded)
    {
        auto const& d = arduboy.savedata;
        auto fname = savedata_filename();
        Text("Hash: %016" PRIx64, arduboy.game_hash);
        NewLine();
        if(std::filesystem::exists(fname))
        {
            TextUnformatted("Save file found.");
#ifdef __EMSCRIPTEN__
            SameLine();
            if(SmallButton("Download"))
            {
                file_download(
                    fname.c_str(),
                    std::filesystem::path(fname).filename().c_str(),
                    "application/octet-stream");
            }
#else
            SameLine();
            if(SmallButton("Open save directory"))
            {
                platform_open_url(std::filesystem::path(fname).parent_path().string().c_str());
            }
#endif
            if(!d.fx_sectors.empty())
            {
                TextUnformatted("Modified flash sectors:");
                Text("   ");
                size_t i = 0;
                for(auto const& [k, v] : d.fx_sectors)
                {
                    SameLine();
                    Text("0x%03x%s", k, ++i == d.fx_sectors.size() ? "" : ",");
                }
            }
            if(d.eeprom.size() == 1024 && d.eeprom_modified_bytes.any())
            {
                TextUnformatted("Modified EEPROM bytes:");
                Text("   ");
                size_t i = 0;
                size_t a = 0;
                std::vector<std::pair<size_t, size_t>> ranges;
                while(a < 1024)
                {
                    if(!d.eeprom_modified_bytes.test(a))
                    {
                        ++a;
                        continue;
                    }
                    size_t b = a;
                    while(b + 1 < 1024 && d.eeprom_modified_bytes.test(b + 1))
                        ++b;
                    ranges.push_back({ a, b });
                    a = b + 1;
                }
                for(auto const& r : ranges)
                {
                    SameLine();
                    char const* comma = (++i == ranges.size() ? "" : ",");
                    if(r.first == r.second)
                        Text("%d%s", (int)r.first, comma);
                    else
                        Text("%d-%d%s", (int)r.first, (int)r.second, comma);
                }
            }
            NewLine();
            if(SmallButton("Delete save file"))
                open_confirm_delete_popup = true;
        }
        else
        {
            TextUnformatted("No save file found.");
        }
    }
    End();

    if(open_confirm_delete_popup)
        OpenPopup(DELETE_POPUP);

    if(BeginPopupModal(DELETE_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        TextUnformatted("Are you sure you want to delete the save file for this game?");
        NewLine();
        if(Button("Delete"))
        {
            auto fname = savedata_filename();
            std::error_code ec{};
            std::filesystem::remove(fname, ec);
#ifdef __EMSCRIPTEN__
            EM_ASM(
                FS.syncfs(function(err) {});
            );
#endif
            CloseCurrentPopup();
            if(arduboy.cpu.decoded)
            {
                arduboy.reset();
                load_savedata();
            }
        }
        SameLine();
        if(Button("Keep save file (do not delete)", { -1.f, 0.f }))
        {
            CloseCurrentPopup();
        }
        EndPopup();
    }
}
