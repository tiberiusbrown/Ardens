#include "imgui.h"
#include "common.hpp"

#include "ImGuiColorTextEdit/TextEditor.h"
#include "dwarf.hpp"

#include <fstream>
#include <streambuf>

static TextEditor editor;

static void init_texteditor()
{
    static bool did_init = false;
    if(did_init) return;

    auto lang = TextEditor::LanguageDefinition::CPlusPlus();
    editor.SetLanguageDefinition(lang);

    editor.SetReadOnly(true);
    editor.SetShowWhitespaces(false);
    editor.SetTabSize(4);
    editor.SetHandleKeyboardInputs(false);
    editor.SetHandleMouseInputs(false);
    editor.SetImGuiChildIgnored(true);

    did_init = true;
}

static void load_file_to_editor(absim::elf_data_t::source_file_t const& sf)
{
    static std::string curr_fname;
    if(curr_fname == sf.filename) return;
    if(sf.filename.empty())
    {
        curr_fname.clear();
        editor.SetText("");
        return;
    }
    curr_fname = sf.filename;

    editor.SetTextLines(sf.lines);
}

struct line_info_t
{
    std::string FileName;
    int Line;
};

#ifdef ARDENS_LLVM
static line_info_t get_line_info(llvm::DWARFContext& dwarf, uint64_t addr)
{
    line_info_t r;
    auto* cu = dwarf.getCompileUnitForAddress(addr);
    if(!cu) return {};

    if(auto const* table = dwarf.getLineTableForUnit(cu))
    {
        for(auto const& seq : table->Sequences)
        {
            if(seq.containsPC({ addr }))
            {
                llvm::DWARFDebugLine::Row row;
                row.Address = { addr };
                auto first_row = table->Rows.begin() + seq.FirstRowIndex;
                auto last_row = table->Rows.begin() + seq.LastRowIndex;
                auto row_pos = std::upper_bound(first_row + 1, last_row - 1, row,
                    llvm::DWARFDebugLine::Row::orderByAddress) - 1;
                uint32_t row_index = uint32_t(row_pos - table->Rows.begin());
                auto const& tr = table->Rows[row_index];
                table->getFileNameByIndex(tr.File,
                    cu->getCompilationDir(),
                    llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                     r.FileName);
                r.Line = tr.Line;
            }
        }
    }

    return r;
}
#endif

void window_source(bool& open)
{
    using namespace ImGui;
    if(!open) return;

    static uint16_t prev_pc = -1;

    SetNextWindowSize({ 400 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Source##SourceWindow", &open)
        && arduboy.cpu.decoded && arduboy.elf && arduboy.paused)
    {
        auto pc = arduboy.cpu.pc;

#ifdef ARDENS_LLVM
        init_texteditor();
        auto& dwarf = *arduboy.elf->dwarf_ctx;
        //auto info = dwarf.getLineInfoForAddress(
        //    { uint64_t(pc) * 2 },
        //    { llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath });
        auto info = get_line_info(dwarf, uint64_t(pc) * 2);
       
        auto it = arduboy.elf->source_file_names.find(info.FileName);
        if(it != arduboy.elf->source_file_names.end() &&
            it->second >= 0 && it->second < arduboy.elf->source_files.size())
        {
            load_file_to_editor(arduboy.elf->source_files[it->second]);
            editor.Render(info.FileName.c_str());
            if(prev_pc != pc)
            {
                editor.SetCursorPosition({ 0, 0 });
                editor.SetCursorPosition({ (int)info.Line - 1, 0 });
            }
            prev_pc = pc;
        }
#endif
     }
    End();
}
