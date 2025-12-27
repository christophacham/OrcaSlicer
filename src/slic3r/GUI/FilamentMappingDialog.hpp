#ifndef slic3r_FilamentMappingDialog_hpp_
#define slic3r_FilamentMappingDialog_hpp_

#include <wx/dialog.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/colour.h>
#include <vector>
#include <string>

namespace Slic3r { namespace GUI {

// Information about a filament used in the project
struct ProjectFilamentInfo {
    int index{0};              // 0-based filament index (T0, T1, etc.)
    std::string name;          // e.g., "Generic PLA"
    std::string type;          // e.g., "PLA", "PETG", "ABS"
    wxColour color;            // Filament color
    double usage_grams{0.0};   // Estimated usage in grams

    wxString display_name() const {
        return wxString::Format("T%d: %s", index, wxString::FromUTF8(name));
    }
};

/**
 * Dialog for mapping project filaments to physical printer slots.
 *
 * Shows a list of filaments used in the project with dropdowns
 * to select which physical slot each filament should use.
 *
 * Usage:
 *   std::vector<ProjectFilamentInfo> filaments = ...;
 *   FilamentMappingDialog dlg(parent, filaments, slot_count);
 *   if (dlg.ShowModal() == wxID_OK) {
 *       auto mapping = dlg.get_mapping();
 *       // mapping[i] = slot number (1-based) for filament i
 *   }
 */
class FilamentMappingDialog : public wxDialog {
public:
    FilamentMappingDialog(wxWindow* parent,
                          const std::vector<ProjectFilamentInfo>& project_filaments,
                          int slot_count,
                          const wxString& title = _L("Filament Mapping"));

    virtual ~FilamentMappingDialog() = default;

    // Get the resulting mapping: filament index -> slot number (1-based)
    std::vector<int> get_mapping() const;

    // Set initial mapping
    void set_mapping(const std::vector<int>& mapping);

private:
    void create_ui();
    void on_auto_match(wxCommandEvent& event);
    void on_reset(wxCommandEvent& event);
    void on_choice_changed(wxCommandEvent& event);

    // Auto-match filaments to slots by type (simple algorithm)
    void auto_match_filaments();

    // Reset to 1:1 default mapping (T0->Slot1, T1->Slot2, etc.)
    void reset_to_default();

    // Update warning label if there are mapping conflicts
    void update_warnings();

    std::vector<ProjectFilamentInfo> m_filaments;
    int m_slot_count;

    // UI elements - one choice per filament
    std::vector<wxChoice*> m_slot_choices;
    std::vector<wxPanel*> m_color_swatches;

    wxStaticText* m_warning_label{nullptr};
    wxButton* m_btn_auto_match{nullptr};
    wxButton* m_btn_reset{nullptr};
};

// Helper function to get slot count from printer config
int get_material_slot_count_from_config(const class DynamicPrintConfig* config);

// Helper function to check if printer supports filament mapping
bool supports_filament_mapping_from_config(const class DynamicPrintConfig* config);

}} // namespace Slic3r::GUI

#endif // slic3r_FilamentMappingDialog_hpp_
