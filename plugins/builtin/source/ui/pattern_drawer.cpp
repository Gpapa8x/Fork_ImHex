#include <ui/pattern_drawer.hpp>

#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_wide_string.hpp>

#include <string>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/localization.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin::ui {

    namespace {

        constexpr auto DisplayEndDefault = 50U;
        constexpr auto DisplayEndStep = 50U;

        using namespace ::std::literals::string_literals;

        bool isPatternSelected(u64 address, u64 size) {
            auto currSelection = ImHexApi::HexEditor::getSelection();
            if (!currSelection.has_value())
                return false;

            return Region{ address, size }.overlaps(*currSelection);
        }

        template<typename T>
        auto highlightWhenSelected(u64 address, u64 size, const T &callback) {
            constexpr bool HasReturn = !requires(T t) { { t() } -> std::same_as<void>; };

            auto selected = isPatternSelected(address, size);

            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));

            if constexpr (HasReturn) {
                auto result = callback();

                if (selected)
                    ImGui::PopStyleColor();

                return result;
            } else {
                callback();

                if (selected)
                    ImGui::PopStyleColor();
            }
        }

        template<typename T>
        auto highlightWhenSelected(const pl::ptrn::Pattern& pattern, const T &callback) {
            return highlightWhenSelected(pattern.getOffset(), pattern.getSize(), callback);
        }

        void createLeafNode(const pl::ptrn::Pattern& pattern) {
            ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf |
                                                                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                                ImGuiTreeNodeFlags_SpanFullWidth |
                                                                ImGuiTreeNodeFlags_AllowItemOverlap);
        }

        bool createTreeNode(const pl::ptrn::Pattern& pattern) {
            if (pattern.isSealed()) {
                ImGui::Indent();
                highlightWhenSelected(pattern, [&]{ ImGui::TextUnformatted(pattern.getDisplayName().c_str()); });
                ImGui::Unindent();
                return false;
            }
            else {
                return highlightWhenSelected(pattern, [&]{ return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);});
            }
        }

        void drawTypenameColumn(const pl::ptrn::Pattern& pattern, const std::string& pattern_name) {
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), pattern_name);
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
            ImGui::TableNextColumn();
        }

        void drawNameColumn(const pl::ptrn::Pattern& pattern) {
            highlightWhenSelected(pattern, [&]{ ImGui::TextUnformatted(pattern.getDisplayName().c_str()); });
            ImGui::TableNextColumn();
        }

        void drawColorColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
        }

        void drawOffsetColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
            ImGui::TableNextColumn();
        }

        void drawSizeColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
            ImGui::TableNextColumn();
        }

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                if (auto comment = pattern.getComment(); !comment.empty()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(pattern.getComment().c_str());
                    ImGui::EndTooltip();
                }
            }
        }

        void makeSelectable(const pl::ptrn::Pattern &pattern) {
            ImGui::PushID(static_cast<int>(pattern.getOffset()));
            ImGui::PushID(pattern.getVariableName().c_str());

            if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
            }

            ImGui::SameLine(0, 0);

            ImGui::PopID();
            ImGui::PopID();
        }

        void createDefaultEntry(pl::ptrn::Pattern &pattern) {
            ImGui::TableNextRow();
            createLeafNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::SameLine();
            drawNameColumn(pattern);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName().empty() ? pattern.getTypeName() : pattern.getFormattedName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

    }



    void PatternDrawer::visit(pl::ptrn::PatternArrayDynamic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternArrayStatic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldField& pattern) {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        ImGui::TableNextColumn();

        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);

        auto byteAddr = pattern.getOffset() + pattern.getBitOffset() / 8;
        auto firstBitIdx = pattern.getBitOffset() % 8;
        auto lastBitIdx = firstBitIdx + (pattern.getBitSize() - 1);
        if (firstBitIdx == lastBitIdx)
            ImGui::TextFormatted("0x{0:08X} bit {1}", byteAddr, firstBitIdx);
        else
            ImGui::TextFormatted("0x{0:08X} bits {1} - {2}", byteAddr, firstBitIdx, lastBitIdx);
        ImGui::TableNextColumn();
        if (pattern.getBitSize() == 1)
            ImGui::TextFormatted("{0} bit", pattern.getBitSize());
        else
            ImGui::TextFormatted("{0} bits", pattern.getBitSize());
        ImGui::TableNextColumn();
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
        ImGui::TableNextColumn();

        ImGui::TextFormatted("{}", pattern.getFormattedValue());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfield& pattern) {
        bool open = true;
        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "bitfield");

            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachMember([&] (auto &field) {
                this->draw(field);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternBoolean& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternCharacter& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternEnum& pattern) {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        makeSelectable(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);
        drawOffsetColumn(pattern);
        drawSizeColumn(pattern);
        drawTypenameColumn(pattern, "enum");
        ImGui::TextFormatted("{}", pattern.getFormattedValue());
    }

    void PatternDrawer::visit(pl::ptrn::PatternFloat& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPadding& pattern) {
        // Do nothing
        hex::unused(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPointer& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.getPointedAtPattern()->accept(*this);

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternSigned& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternString& pattern) {
        if (pattern.getSize() > 0)
            createDefaultEntry(pattern);
        }

    void PatternDrawer::visit(pl::ptrn::PatternStruct& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "struct");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachEntry(0, pattern.getMembers().size(), [&](u64, auto *member){
                this->draw(*member);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnion& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "union");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachEntry(0, pattern.getEntryCount(), [&](u64, auto *member) {
                this->draw(*member);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnsigned& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideCharacter& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideString& pattern) {
        if (pattern.getSize() > 0)
            createDefaultEntry(pattern);
    }

    void PatternDrawer::draw(pl::ptrn::Pattern& pattern) {
        if (pattern.isHidden())
            return;

        pattern.accept(*this);
    }

    void PatternDrawer::drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::Iteratable &iteratable, bool isInlined) {
        if (iteratable.getEntryCount() == 0)
            return;

        bool open = true;
        if (!isInlined) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
            ImGui::SameLine(0, 0);

            ImGui::TextUnformatted("[");
            ImGui::SameLine(0, 0);
            ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", iteratable.getEntryCount());
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            u64 chunkCount = 0;
            for (u64 i = 0; i < iteratable.getEntryCount(); i += ChunkSize) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                chunkCount++;

                auto &displayEnd = this->getDisplayEnd(pattern);
                if (chunkCount > displayEnd) {
                    ImGui::Selectable(hex::format("... ({})", "hex.builtin.pattern_drawer.double_click"_lang).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        displayEnd += DisplayEndStep;
                    break;
                } else {
                    auto endIndex = std::min<u64>(iteratable.getEntryCount(), i + ChunkSize);

                    auto startOffset = iteratable.getEntry(i)->getOffset();
                    auto endOffset = iteratable.getEntry(endIndex - 1)->getOffset();
                    auto endSize = iteratable.getEntry(endIndex - 1)->getSize();

                    size_t chunkSize = (endOffset - startOffset) + endSize;

                    auto chunkOpen = highlightWhenSelected(startOffset, ((endOffset + endSize) - startOffset) - 1, [&]{ return ImGui::TreeNodeEx(hex::format("[{} ... {}]", i, endIndex - 1).c_str(), ImGuiTreeNodeFlags_SpanFullWidth); });
                    ImGui::TableNextColumn();
                    drawColorColumn(pattern);
                    ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", startOffset, startOffset + chunkSize - (pattern.getSize() == 0 ? 0 : 1));
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("0x{0:04X}", chunkSize);
                    ImGui::TableNextColumn();
                    ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
                    ImGui::SameLine(0, 0);

                    ImGui::TextUnformatted("[");
                    ImGui::SameLine(0, 0);
                    ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", endIndex - i);
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted("]");

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("[ ... ]");

                    if (chunkOpen) {
                        iteratable.forEachEntry(i, endIndex, [&](u64, auto *entry){
                            this->draw(*entry);
                        });

                        ImGui::TreePop();
                    }
                }
            }

            if (!isInlined)
                ImGui::TreePop();
        }
    }

    u64& PatternDrawer::getDisplayEnd(const pl::ptrn::Pattern& pattern) {
        auto it = this->m_displayEnd.find(&pattern);
        if (it != this->m_displayEnd.end()) {
            return it->second;
        }

        auto [value, success] = this->m_displayEnd.emplace(&pattern, DisplayEndDefault);
        return value->second;
    }

    static bool sortPatterns(const ImGuiTableSortSpecs* sortSpecs, const pl::ptrn::Pattern * left, const pl::ptrn::Pattern * right) {
        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getDisplayName() > right->getDisplayName();
            else
                return left->getDisplayName() < right->getDisplayName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getOffset() > right->getOffset();
            else
                return left->getOffset() < right->getOffset();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getSize() > right->getSize();
            else
                return left->getSize() < right->getSize();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getValue() > right->getValue();
            else
                return left->getValue() < right->getValue();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getTypeName() > right->getTypeName();
            else
                return left->getTypeName() < right->getTypeName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getColor() > right->getColor();
            else
                return left->getColor() < right->getColor();
        }

        return false;
    }

    static bool beginPatternTable(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, std::vector<pl::ptrn::Pattern*> &sortedPatterns, float height) {
        if (ImGui::BeginTable("##Patterntable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.var_name"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.color"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("color"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.offset"_lang,   ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("offset"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.size"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.type"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("type"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.value"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (patterns.empty())
                sortedPatterns.clear();

            if (!patterns.empty() && (sortSpecs->SpecsDirty || sortedPatterns.empty())) {
                sortedPatterns.clear();
                std::transform(patterns.begin(), patterns.end(), std::back_inserter(sortedPatterns), [](const std::shared_ptr<pl::ptrn::Pattern> &pattern) {
                    return pattern.get();
                });

                std::sort(sortedPatterns.begin(), sortedPatterns.end(), [&sortSpecs](pl::ptrn::Pattern *left, pl::ptrn::Pattern *right) -> bool {
                    return sortPatterns(sortSpecs, left, right);
                });

                for (auto &pattern : sortedPatterns)
                    pattern->sort([&sortSpecs](const pl::ptrn::Pattern *left, const pl::ptrn::Pattern *right){
                        return sortPatterns(sortSpecs, left, right);
                    });

                sortSpecs->SpecsDirty = false;
            }

            return true;
        }

        return false;
    }

    void PatternDrawer::draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, float height) {
        if (beginPatternTable(patterns, this->m_sortedPatterns, height)) {
            ImGui::TableHeadersRow();

            for (auto &pattern : this->m_sortedPatterns) {
                this->draw(*pattern);
            }

            ImGui::EndTable();
        }
    }
}
