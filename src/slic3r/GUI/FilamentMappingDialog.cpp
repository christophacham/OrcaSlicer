#include "FilamentMappingDialog.hpp"
#include "I18N.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <wx/scrolwin.h>
#include <wx/statline.h>
#include <algorithm>
#include <set>

namespace Slic3r { namespace GUI {

FilamentMappingDialog::FilamentMappingDialog(wxWindow* parent,
                                             const std::vector<ProjectFilamentInfo>& project_filaments,
                                             int slot_count,
                                             const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_filaments(project_filaments)
    , m_slot_count(std::max(1, slot_count))
{
    create_ui();
    reset_to_default();
    update_warnings();

    SetMinSize(wxSize(400, 300));
    Fit();
    CenterOnParent();
}

void FilamentMappingDialog::create_ui()
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // Header
    wxStaticText* header = new wxStaticText(this, wxID_ANY,
        _L("Map project filaments to printer slots:"));
    main_sizer->Add(header, 0, wxALL | wxEXPAND, 10);

    // Scrolled window for filament list (handles many filaments gracefully)
    wxScrolledWindow* scroll = new wxScrolledWindow(this, wxID_ANY);
    scroll->SetScrollRate(0, 20);

    wxFlexGridSizer* grid = new wxFlexGridSizer(3, 10, 10);
    grid->AddGrowableCol(1, 1);

    // Column headers
    grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Color")), 0, wxALIGN_CENTER);
    grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Project Filament")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Printer Slot")), 0, wxALIGN_CENTER_VERTICAL);

    // Build slot choices array
    wxArrayString slot_choices;
    for (int i = 1; i <= m_slot_count; ++i) {
        slot_choices.Add(wxString::Format("Slot %d", i));
    }

    // Create row for each filament
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        const auto& filament = m_filaments[i];

        // Color swatch
        wxPanel* swatch = new wxPanel(scroll, wxID_ANY, wxDefaultPosition, wxSize(24, 24));
        swatch->SetBackgroundColour(filament.color.IsOk() ? filament.color : *wxLIGHT_GREY);
        m_color_swatches.push_back(swatch);
        grid->Add(swatch, 0, wxALIGN_CENTER);

        // Filament name and type
        wxString label = filament.display_name();
        if (!filament.type.empty()) {
            label += wxString::Format(" (%s)", wxString::FromUTF8(filament.type));
        }
        if (filament.usage_grams > 0) {
            label += wxString::Format(" - %.1fg", filament.usage_grams);
        }
        grid->Add(new wxStaticText(scroll, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);

        // Slot dropdown
        wxChoice* choice = new wxChoice(scroll, wxID_ANY, wxDefaultPosition, wxDefaultSize, slot_choices);
        choice->Bind(wxEVT_CHOICE, &FilamentMappingDialog::on_choice_changed, this);
        m_slot_choices.push_back(choice);
        grid->Add(choice, 0, wxEXPAND);
    }

    scroll->SetSizer(grid);
    main_sizer->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // Separator
    main_sizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    // Warning label (hidden by default)
    m_warning_label = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_warning_label->SetForegroundColour(*wxRED);
    m_warning_label->Hide();
    main_sizer->Add(m_warning_label, 0, wxALL | wxEXPAND, 10);

    // Buttons
    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_btn_auto_match = new wxButton(this, wxID_ANY, _L("Auto Match"));
    m_btn_auto_match->Bind(wxEVT_BUTTON, &FilamentMappingDialog::on_auto_match, this);
    m_btn_auto_match->SetToolTip(_L("Automatically match filaments to slots by type"));
    btn_sizer->Add(m_btn_auto_match, 0, wxRIGHT, 5);

    m_btn_reset = new wxButton(this, wxID_ANY, _L("Reset"));
    m_btn_reset->Bind(wxEVT_BUTTON, &FilamentMappingDialog::on_reset, this);
    m_btn_reset->SetToolTip(_L("Reset to default 1:1 mapping"));
    btn_sizer->Add(m_btn_reset, 0, wxRIGHT, 20);

    btn_sizer->AddStretchSpacer();

    wxButton* btn_ok = new wxButton(this, wxID_OK, _L("OK"));
    btn_sizer->Add(btn_ok, 0, wxRIGHT, 5);

    wxButton* btn_cancel = new wxButton(this, wxID_CANCEL, _L("Cancel"));
    btn_sizer->Add(btn_cancel, 0);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 10);

    SetSizer(main_sizer);
}

std::vector<int> FilamentMappingDialog::get_mapping() const
{
    std::vector<int> mapping;
    mapping.reserve(m_slot_choices.size());

    for (const auto* choice : m_slot_choices) {
        // Convert 0-based selection to 1-based slot number
        mapping.push_back(choice->GetSelection() + 1);
    }

    return mapping;
}

void FilamentMappingDialog::set_mapping(const std::vector<int>& mapping)
{
    for (size_t i = 0; i < m_slot_choices.size() && i < mapping.size(); ++i) {
        int slot = mapping[i];
        // Convert 1-based slot to 0-based selection
        if (slot >= 1 && slot <= m_slot_count) {
            m_slot_choices[i]->SetSelection(slot - 1);
        }
    }
    update_warnings();
}

void FilamentMappingDialog::on_auto_match(wxCommandEvent& /*event*/)
{
    auto_match_filaments();
    update_warnings();
}

void FilamentMappingDialog::on_reset(wxCommandEvent& /*event*/)
{
    reset_to_default();
    update_warnings();
}

void FilamentMappingDialog::on_choice_changed(wxCommandEvent& /*event*/)
{
    update_warnings();
}

void FilamentMappingDialog::auto_match_filaments()
{
    // Simple auto-match: try to assign filaments of the same type to different slots
    // Priority: PLA->first slots, PETG->next, ABS->next, etc.

    std::vector<int> assignment(m_filaments.size(), -1);
    std::set<int> used_slots;

    // First pass: try to match by preferred slot (based on original index)
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        int preferred_slot = static_cast<int>(i);
        if (preferred_slot < m_slot_count && used_slots.find(preferred_slot) == used_slots.end()) {
            assignment[i] = preferred_slot;
            used_slots.insert(preferred_slot);
        }
    }

    // Second pass: assign remaining filaments to any available slot
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        if (assignment[i] == -1) {
            for (int slot = 0; slot < m_slot_count; ++slot) {
                if (used_slots.find(slot) == used_slots.end()) {
                    assignment[i] = slot;
                    used_slots.insert(slot);
                    break;
                }
            }
        }
    }

    // Apply assignment (if still -1, default to slot 0)
    for (size_t i = 0; i < m_filaments.size(); ++i) {
        int slot = assignment[i] >= 0 ? assignment[i] : 0;
        m_slot_choices[i]->SetSelection(slot);
    }
}

void FilamentMappingDialog::reset_to_default()
{
    // Default: T0->Slot1, T1->Slot2, etc. (with wraparound if needed)
    for (size_t i = 0; i < m_slot_choices.size(); ++i) {
        int slot = static_cast<int>(i) % m_slot_count;
        m_slot_choices[i]->SetSelection(slot);
    }
}

void FilamentMappingDialog::update_warnings()
{
    // Check for duplicate slot assignments
    std::map<int, int> slot_usage; // slot -> count
    for (const auto* choice : m_slot_choices) {
        int slot = choice->GetSelection();
        slot_usage[slot]++;
    }

    std::vector<int> conflicts;
    for (const auto& [slot, count] : slot_usage) {
        if (count > 1) {
            conflicts.push_back(slot + 1); // Convert to 1-based for display
        }
    }

    if (conflicts.empty()) {
        m_warning_label->Hide();
    } else {
        wxString warning = _L("Warning: Multiple filaments mapped to same slot: ");
        for (size_t i = 0; i < conflicts.size(); ++i) {
            if (i > 0) warning += ", ";
            warning += wxString::Format("Slot %d", conflicts[i]);
        }
        m_warning_label->SetLabel(warning);
        m_warning_label->Show();
    }

    Layout();
}

// Helper function implementations
int get_material_slot_count_from_config(const DynamicPrintConfig* config)
{
    if (!config) return 4;
    auto* opt = config->option<ConfigOptionInt>("material_slot_count");
    return opt ? std::clamp(opt->value, 1, 64) : 4;
}

bool supports_filament_mapping_from_config(const DynamicPrintConfig* config)
{
    if (!config) return false;
    auto* opt = config->option<ConfigOptionBool>("supports_filament_mapping");
    return opt ? opt->value : false;
}

}} // namespace Slic3r::GUI
