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
    if(Begin("Save File", &open) && arduboy->cpu.decoded)
    {
        auto const& d = arduboy->savedata;
        auto fname = savedata_filename();
        Text("Game Hash: %016" PRIx64, d.game_hash);
        NewLine();
        if(std::filesystem::exists(fname))
        {
            TextUnformatted("Save file found.");
            SameLine();
            if(SmallButton("Delete save file"))
                open_confirm_delete_popup = true;
            if(!d.fx_sectors.empty())
            {
                TextUnformatted("Modified flash sectors:");
                Text("   ");
                size_t i = 0;
                for(auto const& [k, v] : d.fx_sectors)
                {
                    SameLine();
                    Text("%04x%s", k, ++i == d.fx_sectors.size() ? "" : ",");
                }
            }
            if(d.eeprom.size() == 1024 && d.eeprom_modified_bytes.any())
            {
                TextUnformatted("Modified EEPROM addresses:");
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
                        Text("%d%s", r.first, comma);
                    else
                        Text("%d-%d%s", r.first, r.second, comma);
                }
            }
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
        if(Button("Yes"))
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
        }
        SameLine();
        if(Button("No", { -1.f, 0.f }))
        {
            CloseCurrentPopup();
        }
        EndPopup();
    }
}
