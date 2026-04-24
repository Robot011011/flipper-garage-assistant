#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TAG "GarageTest"
#define GARAGE_TEST_DATA_DIR EXT_PATH("apps_data/garage_test")
#define GARAGE_TEST_DATA_FILE EXT_PATH("apps_data/garage_test/garage_test.txt")
#define GARAGE_TEST_MAX_PROFILES 5
#define GARAGE_TEST_NAME_LEN 13
#define GARAGE_TEST_STORAGE_BUFFER_SIZE 2048

typedef struct {
    char name[GARAGE_TEST_NAME_LEN];
    uint32_t oil_miles;
    uint32_t chain_miles;
    uint32_t oil_interval;
    uint32_t chain_interval;
    uint32_t front_psi;
    uint32_t rear_psi;
} BikeProfile;

typedef struct {
    FuriMessageQueue* input_queue;
    uint8_t screen;
    bool edit_mode;
    bool profile_menu_mode;
    bool settings_menu_mode;
    bool settings_edit_mode;
    bool rename_mode;
    bool max_bikes_message_mode;
    bool delete_select_mode;
    bool delete_confirm_mode;
    bool delete_final_confirm_mode;
    bool delete_blocked_mode;
    uint8_t selected_item;
    uint8_t profile_menu_selection;
    uint8_t settings_selection;
    uint8_t delete_selection;
    BikeProfile profiles[GARAGE_TEST_MAX_PROFILES];
    uint8_t profile_count;
    uint8_t active_profile_index;
    uint32_t oil_last_reset;
    uint32_t chain_last_reset;
    uint32_t edit_value;
    uint32_t settings_edit_value;
    char name_edit_buffer[GARAGE_TEST_NAME_LEN];
    uint8_t name_edit_cursor;
} GarageTestApp;

static BikeProfile* garage_test_active_profile(GarageTestApp* app) {
    return &app->profiles[app->active_profile_index];
}

static const BikeProfile* garage_test_active_profile_const(const GarageTestApp* app) {
    return &app->profiles[app->active_profile_index];
}

static void garage_test_copy_string(char* destination, size_t size, const char* source);

static void garage_test_set_profile_count(GarageTestApp* app, unsigned long profile_count) {
    // Always keep at least one profile; future delete logic must not remove the final profile.
    if(profile_count < 1) profile_count = 1;
    if(profile_count > GARAGE_TEST_MAX_PROFILES) profile_count = GARAGE_TEST_MAX_PROFILES;

    app->profile_count = (uint8_t)profile_count;
    if(app->active_profile_index >= app->profile_count) app->active_profile_index = 0;
}

static void garage_test_init_profile(
    BikeProfile* profile,
    const char* name,
    uint32_t oil_miles,
    uint32_t chain_miles,
    uint32_t oil_interval,
    uint32_t chain_interval,
    uint32_t front_psi,
    uint32_t rear_psi) {
    garage_test_copy_string(profile->name, sizeof(profile->name), name);
    profile->oil_miles = oil_miles;
    profile->chain_miles = chain_miles;
    profile->oil_interval = oil_interval;
    profile->chain_interval = chain_interval;
    profile->front_psi = front_psi;
    profile->rear_psi = rear_psi;
}

static void garage_test_delete_profile(GarageTestApp* app, uint8_t profile_index) {
    if(app->profile_count <= 1 || profile_index >= app->profile_count) return;

    uint8_t old_active_index = app->active_profile_index;
    for(size_t i = profile_index; i < ((size_t)app->profile_count - 1); i++) {
        app->profiles[i] = app->profiles[i + 1];
    }
    garage_test_init_profile(
        &app->profiles[app->profile_count - 1], "", 0, 0, 3000, 500, 32, 36);
    garage_test_set_profile_count(app, app->profile_count - 1);

    if(old_active_index == profile_index) {
        app->active_profile_index =
            (profile_index < app->profile_count) ? profile_index : app->profile_count - 1;
    } else if(old_active_index > profile_index) {
        app->active_profile_index = old_active_index - 1;
    } else {
        app->active_profile_index = old_active_index;
    }

    if(app->active_profile_index >= app->profile_count) app->active_profile_index = 0;
}

static bool garage_test_profile_name_exists(const GarageTestApp* app, const char* name) {
    for(size_t i = 0; i < app->profile_count; i++) {
        if(strcmp(app->profiles[i].name, name) == 0) return true;
    }

    return false;
}

static void garage_test_generate_new_bike_name(const GarageTestApp* app, char* output, size_t size) {
    if(!garage_test_profile_name_exists(app, "New Bike")) {
        garage_test_copy_string(output, size, "New Bike");
        return;
    }

    for(size_t suffix = 2; suffix <= GARAGE_TEST_MAX_PROFILES; suffix++) {
        char candidate[GARAGE_TEST_NAME_LEN];
        snprintf(candidate, sizeof(candidate), "New Bike %u", (unsigned)suffix);
        if(!garage_test_profile_name_exists(app, candidate)) {
            garage_test_copy_string(output, size, candidate);
            return;
        }
    }

    garage_test_copy_string(output, size, "New Bike");
}

static uint32_t garage_test_settings_value(const GarageTestApp* app, uint8_t selection) {
    const BikeProfile* profile = garage_test_active_profile_const(app);

    if(selection == 0) return profile->oil_interval;
    if(selection == 1) return profile->chain_interval;
    if(selection == 2) return profile->front_psi;
    return profile->rear_psi;
}

static uint32_t garage_test_settings_step(uint8_t selection) {
    if(selection == 0) return 500;
    if(selection == 1) return 100;
    return 1;
}

static uint32_t garage_test_settings_min(uint8_t selection) {
    if(selection == 0 || selection == 1) return 100;
    return 1;
}

static const char* garage_test_settings_label(uint8_t selection) {
    if(selection == 0) return "Oil Interval";
    if(selection == 1) return "Chain Interval";
    if(selection == 2) return "Front PSI";
    return "Rear PSI";
}

static void garage_test_apply_settings_value(GarageTestApp* app, uint8_t selection, uint32_t value) {
    BikeProfile* profile = garage_test_active_profile(app);

    if(selection == 0) profile->oil_interval = value;
    else if(selection == 1) profile->chain_interval = value;
    else if(selection == 2) profile->front_psi = value;
    else profile->rear_psi = value;
}

static char garage_test_next_name_char(char current, bool forward) {
    static const char charset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t charset_len = strlen(charset);
    size_t index = 0;

    for(size_t i = 0; i < charset_len; i++) {
        if(charset[i] == current) {
            index = i;
            break;
        }
    }

    if(forward) {
        index = (index + 1) % charset_len;
    } else {
        index = (index == 0) ? (charset_len - 1) : (index - 1);
    }

    return charset[index];
}

static void garage_test_trim_trailing_spaces(char* text) {
    size_t len = strlen(text);
    while(len > 0 && text[len - 1] == ' ') {
        text[len - 1] = '\0';
        len--;
    }
}

static void garage_test_copy_string(char* destination, size_t size, const char* source) {
    if(size == 0) return;
    snprintf(destination, size, "%s", source ? source : "");
}

static void garage_test_ensure_profile_names(GarageTestApp* app) {
    for(size_t i = 0; i < app->profile_count; i++) {
        if(app->profiles[i].name[0] == '\0') {
            char fallback_name[GARAGE_TEST_NAME_LEN];
            snprintf(fallback_name, sizeof(fallback_name), "Bike %u", (unsigned)(i + 1));
            garage_test_copy_string(
                app->profiles[i].name, sizeof(app->profiles[i].name), fallback_name);
        }
    }
}

static void garage_test_init_default_profiles(GarageTestApp* app) {
    for(size_t i = 0; i < GARAGE_TEST_MAX_PROFILES; i++) {
        garage_test_init_profile(&app->profiles[i], "", 0, 0, 3000, 500, 32, 36);
    }
    garage_test_init_profile(&app->profiles[0], "Goku R7", 0, 0, 3000, 500, 36, 42);
    garage_test_init_profile(&app->profiles[1], "Shop Bike", 0, 0, 3000, 500, 32, 36);
    app->active_profile_index = 0;
    garage_test_set_profile_count(app, 2);
    app->oil_last_reset = 0;
    app->chain_last_reset = 0;
}

static void garage_test_load_values(GarageTestApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage for load");
        garage_test_init_default_profiles(app);
        return;
    }

    FURI_LOG_I(TAG, "Ensuring data dir exists: %s", GARAGE_TEST_DATA_DIR);
    storage_simply_mkdir(storage, GARAGE_TEST_DATA_DIR);

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to allocate file for load");
        furi_record_close(RECORD_STORAGE);
        garage_test_init_default_profiles(app);
        return;
    }

    garage_test_init_default_profiles(app);

    FURI_LOG_I(TAG, "Loading values from %s", GARAGE_TEST_DATA_FILE);
    if(storage_file_open(file, GARAGE_TEST_DATA_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[GARAGE_TEST_STORAGE_BUFFER_SIZE];
        size_t bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';

        char* cursor = buffer;
        while(cursor && *cursor) {
            char* line = cursor;
            char* next_line = strchr(cursor, '\n');
            if(next_line) {
                *next_line = '\0';
                cursor = next_line + 1;
            } else {
                cursor = NULL;
            }

            char* value = strchr(line, '=');
            if(!value) continue;

            *value = '\0';
            value++;

            if(strcmp(line, "profile_count") == 0) {
                unsigned long parsed_value = 0;
                if(sscanf(value, "%lu", &parsed_value) == 1) {
                    garage_test_set_profile_count(app, parsed_value);
                    FURI_LOG_I(TAG, "Loaded profile_count=%lu", parsed_value);
                } else {
                    FURI_LOG_E(TAG, "Failed to parse profile_count");
                }
            } else if(strcmp(line, "active_profile_index") == 0) {
                unsigned long parsed_value = 0;
                if(sscanf(value, "%lu", &parsed_value) == 1) {
                    app->active_profile_index =
                        (parsed_value < app->profile_count) ? (uint8_t)parsed_value : 0;
                    FURI_LOG_I(TAG, "Loaded active_profile_index=%lu", parsed_value);
                } else {
                    FURI_LOG_E(TAG, "Failed to parse active_profile_index");
                }
            } else if(strncmp(line, "profile", 7) == 0) {
                unsigned long parsed_value = 0;
                unsigned long profile_index = 0;
                char field[24];
                if(sscanf(line, "profile%lu_%23s", &profile_index, field) == 2) {
                    if(profile_index >= GARAGE_TEST_MAX_PROFILES) {
                        FURI_LOG_E(TAG, "Profile index out of range in key '%s'", line);
                    } else if(strcmp(field, "name") == 0) {
                        garage_test_copy_string(
                            app->profiles[profile_index].name, sizeof(app->profiles[profile_index].name), value);
                        FURI_LOG_I(TAG, "Loaded profile%lu_name=%s", profile_index, app->profiles[profile_index].name);
                    } else if(sscanf(value, "%lu", &parsed_value) == 1) {
                        if(strcmp(field, "oil") == 0) app->profiles[profile_index].oil_miles = (uint32_t)parsed_value;
                        else if(strcmp(field, "chain") == 0) app->profiles[profile_index].chain_miles = (uint32_t)parsed_value;
                        else if(strcmp(field, "oil_interval") == 0) app->profiles[profile_index].oil_interval = (uint32_t)parsed_value;
                        else if(strcmp(field, "chain_interval") == 0) app->profiles[profile_index].chain_interval = (uint32_t)parsed_value;
                        else if(strcmp(field, "front_psi") == 0) app->profiles[profile_index].front_psi = (uint32_t)parsed_value;
                        else if(strcmp(field, "rear_psi") == 0) app->profiles[profile_index].rear_psi = (uint32_t)parsed_value;
                        else FURI_LOG_E(TAG, "Unknown key '%s'", line);
                    } else {
                        FURI_LOG_E(TAG, "Failed to parse value for key '%s'", line);
                    }
                } else {
                    FURI_LOG_E(TAG, "Failed to parse profile key '%s'", line);
                }
            } else {
                FURI_LOG_E(TAG, "Unknown key '%s'", line);
            }
        }

        garage_test_set_profile_count(app, app->profile_count);
        garage_test_ensure_profile_names(app);
    } else {
        FURI_LOG_I(TAG, "No existing data file, starting at 0");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void garage_test_save_values(const GarageTestApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage for save");
        return;
    }

    FURI_LOG_I(TAG, "Ensuring data dir exists: %s", GARAGE_TEST_DATA_DIR);
    if(!storage_simply_mkdir(storage, GARAGE_TEST_DATA_DIR)) {
        FURI_LOG_E(TAG, "Failed to create data dir");
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to allocate file for save");
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FURI_LOG_I(TAG, "Saving values to %s", GARAGE_TEST_DATA_FILE);
    if(storage_file_open(file, GARAGE_TEST_DATA_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buffer[GARAGE_TEST_STORAGE_BUFFER_SIZE];
        size_t offset = 0;
        int length = snprintf(
            buffer + offset,
            sizeof(buffer) - offset,
            "profile_count=%u\n\n",
            app->profile_count);

        if(length > 0 && (size_t)length < (sizeof(buffer) - offset)) {
            offset += (size_t)length;
            bool success = true;

            for(size_t i = 0; i < app->profile_count; i++) {
                length = snprintf(
                    buffer + offset,
                    sizeof(buffer) - offset,
                    "profile%u_name=%s\n"
                    "profile%u_oil=%lu\n"
                    "profile%u_chain=%lu\n"
                    "profile%u_oil_interval=%lu\n"
                    "profile%u_chain_interval=%lu\n"
                    "profile%u_front_psi=%lu\n"
                    "profile%u_rear_psi=%lu\n\n",
                    (unsigned)i,
                    app->profiles[i].name,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].oil_miles,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].chain_miles,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].oil_interval,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].chain_interval,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].front_psi,
                    (unsigned)i,
                    (unsigned long)app->profiles[i].rear_psi);

                if(length <= 0 || (size_t)length >= (sizeof(buffer) - offset)) {
                    success = false;
                    break;
                }
                offset += (size_t)length;
            }

            if(success) {
                length = snprintf(
                    buffer + offset,
                    sizeof(buffer) - offset,
                    "active_profile_index=%u\n",
                    app->active_profile_index);
            }

            if(success && length > 0 && (size_t)length < (sizeof(buffer) - offset)) {
                offset += (size_t)length;
            } else if(success) {
                success = false;
            }

            if(success && storage_file_write(file, buffer, offset) == offset) {
                storage_file_sync(file);
                FURI_LOG_I(
                    TAG,
                    "Saved %u profiles, active=%u",
                    app->profile_count,
                    app->active_profile_index);
            } else {
                FURI_LOG_E(TAG, "Failed to write values");
            }
        } else {
            FURI_LOG_E(TAG, "Failed to format save buffer");
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open data file for save");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void garage_test_draw_callback(Canvas* canvas, void* context) {
    GarageTestApp* app = context;
    const BikeProfile* profile = garage_test_active_profile_const(app);
    char line[32];
    char last_line[32];
    const char* page_label = NULL;
    uint32_t miles_left = 0;

    canvas_clear(canvas);

    if(
        !app->rename_mode && !app->profile_menu_mode && !app->settings_menu_mode &&
        !app->settings_edit_mode && !app->max_bikes_message_mode && !app->delete_select_mode &&
        !app->delete_confirm_mode && !app->delete_final_confirm_mode && !app->delete_blocked_mode) {
        page_label = "[1/3]";
    }

    if(
        !app->rename_mode && !app->profile_menu_mode && !app->settings_menu_mode &&
        !app->settings_edit_mode && !app->max_bikes_message_mode && !app->delete_select_mode &&
        !app->delete_confirm_mode && !app->delete_final_confirm_mode && !app->delete_blocked_mode &&
        (app->screen == 1)) {
        page_label = "[2/3]";
    } else if(
        !app->rename_mode && !app->profile_menu_mode && !app->settings_menu_mode &&
        !app->settings_edit_mode && !app->max_bikes_message_mode && !app->delete_select_mode &&
        !app->delete_confirm_mode && !app->delete_final_confirm_mode && !app->delete_blocked_mode &&
        (app->screen == 2)) {
        page_label = "[3/3]";
    }

    if(page_label) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, page_label);
    }

    if(app->rename_mode) {
        char prefix[GARAGE_TEST_NAME_LEN];
        size_t prefix_len = app->name_edit_cursor;
        size_t name_len = strlen(app->name_edit_buffer);

        if(prefix_len > name_len) prefix_len = name_len;
        memcpy(prefix, app->name_edit_buffer, prefix_len);
        prefix[prefix_len] = '\0';

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Rename Bike");
        canvas_set_font(canvas, FontSecondary);
        size_t text_width = canvas_string_width(canvas, app->name_edit_buffer);
        size_t prefix_width = canvas_string_width(canvas, prefix);
        size_t char_width = 0;
        char current_char[2] = {' ', '\0'};
        int32_t name_x = (128 - (int32_t)text_width) / 2;
        int32_t cursor_x = name_x + (int32_t)prefix_width;

        if(app->name_edit_cursor < name_len) current_char[0] = app->name_edit_buffer[app->name_edit_cursor];
        char_width = canvas_string_width(canvas, current_char);
        if(char_width == 0) char_width = canvas_string_width(canvas, " ");

        canvas_draw_str(canvas, name_x, 30, app->name_edit_buffer);
        canvas_draw_line(
            canvas,
            cursor_x,
            36,
            cursor_x + (int32_t)char_width - 1,
            36);
        canvas_draw_line(canvas, 18, 38, 110, 38);
        snprintf(line, sizeof(line), "Cursor: %u", (unsigned)(app->name_edit_cursor + 1));
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK save  Back cancel");
    } else if(app->profile_menu_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Profile Edit");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            line,
            sizeof(line),
            "%c Rename Bike",
            app->profile_menu_selection == 0 ? '>' : ' ');
        canvas_draw_str_aligned(canvas, 4, 22, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Edit Settings",
            app->profile_menu_selection == 1 ? '>' : ' ');
        canvas_draw_str_aligned(canvas, 4, 34, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Add New Bike",
            app->profile_menu_selection == 2 ? '>' : ' ');
        canvas_draw_str_aligned(canvas, 4, 46, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Delete Bike",
            app->profile_menu_selection == 3 ? '>' : ' ');
        canvas_draw_str_aligned(canvas, 4, 56, AlignLeft, AlignCenter, line);
    } else if(app->delete_select_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Delete Bike");
        canvas_set_font(canvas, FontSecondary);
        for(size_t i = 0; i < app->profile_count; i++) {
            snprintf(
                line,
                sizeof(line),
                "%c %s",
                app->delete_selection == i ? '>' : ' ',
                app->profiles[i].name);
            canvas_draw_str_aligned(canvas, 4, 20 + (int32_t)(i * 10), AlignLeft, AlignCenter, line);
        }
    } else if(app->delete_confirm_mode) {
        const char* delete_name = app->profiles[app->delete_selection].name;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Delete Bike?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, delete_name);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Press OK twice to confirm");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back = Cancel");
    } else if(app->delete_final_confirm_mode) {
        const char* delete_name = app->profiles[app->delete_selection].name;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Confirm Delete");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, delete_name);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "OK = Confirm");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back = Cancel");
    } else if(app->delete_blocked_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, "Cannot delete");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "At least 1 bike required");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, "Back = Return");
    } else if(app->max_bikes_message_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Max bikes reached");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "Back: Menu");
    } else if(app->settings_menu_mode && !app->settings_edit_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Edit Settings");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            line,
            sizeof(line),
            "%c Oil Interval:%lu",
            app->settings_selection == 0 ? '>' : ' ',
            (unsigned long)profile->oil_interval);
        canvas_draw_str_aligned(canvas, 4, 20, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Chain Int:%lu",
            app->settings_selection == 1 ? '>' : ' ',
            (unsigned long)profile->chain_interval);
        canvas_draw_str_aligned(canvas, 4, 32, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Front PSI:%lu",
            app->settings_selection == 2 ? '>' : ' ',
            (unsigned long)profile->front_psi);
        canvas_draw_str_aligned(canvas, 4, 44, AlignLeft, AlignCenter, line);
        snprintf(
            line,
            sizeof(line),
            "%c Rear PSI:%lu",
            app->settings_selection == 3 ? '>' : ' ',
            (unsigned long)profile->rear_psi);
        canvas_draw_str_aligned(canvas, 4, 56, AlignLeft, AlignCenter, line);
    } else if(app->settings_edit_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, garage_test_settings_label(app->settings_selection));
        canvas_set_font(canvas, FontSecondary);
        snprintf(line, sizeof(line), "%lu", (unsigned long)app->settings_edit_value);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, line);
        snprintf(line, sizeof(line), "Step: %lu", (unsigned long)garage_test_settings_step(app->settings_selection));
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK save  Back cancel");
    } else if(app->screen == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Garage Assistant");
        canvas_set_font(canvas, FontSecondary);
        snprintf(line, sizeof(line), "> %s", profile->name);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, line);
        canvas_draw_line(canvas, 18, 38, 110, 38);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "OK = switch");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, "Down edit  Right open");
    } else if((app->screen == 1) && !app->edit_mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Service");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, profile->name);
        if(profile->oil_miles >= profile->oil_interval) {
            snprintf(line, sizeof(line), "%c Oil   DUE", app->selected_item == 0 ? '>' : ' ');
            miles_left = 0;
        } else {
            miles_left = profile->oil_interval - profile->oil_miles;
            snprintf(
                line,
                sizeof(line),
                "%c Oil   %lu/%lu",
                app->selected_item == 0 ? '>' : ' ',
                (unsigned long)profile->oil_miles,
                (unsigned long)profile->oil_interval);
        }
        snprintf(last_line, sizeof(last_line), "  Left  %lu miles", (unsigned long)miles_left);
        canvas_draw_str_aligned(canvas, 4, 28, AlignLeft, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 4, 38, AlignLeft, AlignCenter, last_line);
        if(profile->chain_miles >= profile->chain_interval) {
            snprintf(line, sizeof(line), "%c Chain DUE", app->selected_item == 1 ? '>' : ' ');
            miles_left = 0;
        } else {
            miles_left = profile->chain_interval - profile->chain_miles;
            snprintf(
                line,
                sizeof(line),
                "%c Chain %lu/%lu",
                app->selected_item == 1 ? '>' : ' ',
                (unsigned long)profile->chain_miles,
                (unsigned long)profile->chain_interval);
        }
        snprintf(last_line, sizeof(last_line), "  Left  %lu miles", (unsigned long)miles_left);
        canvas_draw_str_aligned(canvas, 4, 46, AlignLeft, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 4, 56, AlignLeft, AlignCenter, last_line);
    } else if(app->screen == 2) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Recommended Tire PSI");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, profile->name);
        snprintf(line, sizeof(line), "Front: %lu PSI", (unsigned long)profile->front_psi);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, line);
        snprintf(line, sizeof(line), "Rear: %lu PSI", (unsigned long)profile->rear_psi);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "Check tires cold");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            64,
            14,
            AlignCenter,
            AlignCenter,
            app->selected_item == 0 ? "Edit Oil" : "Edit Chain");
        canvas_set_font(canvas, FontSecondary);
        snprintf(line, sizeof(line), "%lu miles", (unsigned long)app->edit_value);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, line);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Up/Down: +/-100");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Right: Reset");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "OK: Save");
    }
}

static void garage_test_input_callback(InputEvent* input_event, void* context) {
    GarageTestApp* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

int32_t garage_test_main(void* p) {
    UNUSED(p);

    GarageTestApp* app = calloc(1, sizeof(GarageTestApp));
    if(!app) return -1;

    app->screen = 0;
    app->edit_mode = false;
    app->profile_menu_mode = false;
    app->settings_menu_mode = false;
    app->settings_edit_mode = false;
    app->rename_mode = false;
    app->max_bikes_message_mode = false;
    app->delete_select_mode = false;
    app->delete_confirm_mode = false;
    app->delete_final_confirm_mode = false;
    app->delete_blocked_mode = false;
    app->selected_item = 0;
    app->profile_menu_selection = 0;
    app->settings_selection = 0;
    app->delete_selection = 0;
    garage_test_init_default_profiles(app);
    app->edit_value = 0;
    app->settings_edit_value = 0;
    garage_test_copy_string(app->name_edit_buffer, sizeof(app->name_edit_buffer), "");
    app->name_edit_cursor = 0;
    garage_test_load_values(app);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(!app->input_queue) {
        free(app);
        return -1;
    }

    ViewPort* view_port = view_port_alloc();
    if(!view_port) {
        furi_message_queue_free(app->input_queue);
        free(app);
        return -1;
    }

    view_port_draw_callback_set(view_port, garage_test_draw_callback, app);
    view_port_input_callback_set(view_port, garage_test_input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        view_port_free(view_port);
        furi_message_queue_free(app->input_queue);
        free(app);
        return -1;
    }

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent input_event;
        FuriStatus status =
            furi_message_queue_get(app->input_queue, &input_event, FuriWaitForever);

        if(status != FuriStatusOk) continue;

        if(app->rename_mode) {
            if((input_event.key == InputKeyBack) && (input_event.type == InputTypeShort)) {
                app->rename_mode = false;
                view_port_update(view_port);
            } else if(
                (input_event.key == InputKeyUp) &&
                ((input_event.type == InputTypeShort) || (input_event.type == InputTypeRepeat))) {
                app->name_edit_buffer[app->name_edit_cursor] =
                    garage_test_next_name_char(app->name_edit_buffer[app->name_edit_cursor], true);
                view_port_update(view_port);
            } else if((input_event.key == InputKeyDown) && (input_event.type == InputTypeLong)) {
                /* Long Down is a quick way to insert a space without stepping through the charset. */
                app->name_edit_buffer[app->name_edit_cursor] = ' ';
                view_port_update(view_port);
            } else if(
                (input_event.key == InputKeyDown) &&
                ((input_event.type == InputTypeShort) || (input_event.type == InputTypeRepeat))) {
                app->name_edit_buffer[app->name_edit_cursor] =
                    garage_test_next_name_char(app->name_edit_buffer[app->name_edit_cursor], false);
                view_port_update(view_port);
            } else if((input_event.key == InputKeyRight) && (input_event.type == InputTypeShort)) {
                if(app->name_edit_cursor < (GARAGE_TEST_NAME_LEN - 2)) {
                    app->name_edit_cursor++;
                }
                view_port_update(view_port);
            } else if((input_event.key == InputKeyLeft) && (input_event.type == InputTypeShort)) {
                if(app->name_edit_cursor > 0) {
                    app->name_edit_cursor--;
                }
                view_port_update(view_port);
            } else if((input_event.key == InputKeyOk) && (input_event.type == InputTypeShort)) {
                garage_test_trim_trailing_spaces(app->name_edit_buffer);
                if(app->name_edit_buffer[0] == '\0') {
                    garage_test_copy_string(
                        garage_test_active_profile(app)->name,
                        sizeof(garage_test_active_profile(app)->name),
                        "BIKE");
                } else {
                    garage_test_copy_string(
                        garage_test_active_profile(app)->name,
                        sizeof(garage_test_active_profile(app)->name),
                        app->name_edit_buffer);
                }
                garage_test_save_values(app);
                app->rename_mode = false;
                view_port_update(view_port);
            }
            continue;
        } else if(app->max_bikes_message_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->max_bikes_message_mode = false;
                app->profile_menu_mode = true;
                view_port_update(view_port);
            }
        } else if(app->delete_blocked_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->delete_blocked_mode = false;
                app->profile_menu_mode = true;
                view_port_update(view_port);
            }
        } else if(app->delete_confirm_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->delete_confirm_mode = false;
                app->delete_select_mode = true;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                app->delete_confirm_mode = false;
                app->delete_final_confirm_mode = true;
                view_port_update(view_port);
            }
        } else if(app->delete_final_confirm_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->delete_final_confirm_mode = false;
                app->delete_confirm_mode = true;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                garage_test_delete_profile(app, app->delete_selection);
                garage_test_save_values(app);
                app->delete_final_confirm_mode = false;
                app->screen = 0;
                view_port_update(view_port);
            }
        } else if(app->delete_select_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->delete_select_mode = false;
                app->profile_menu_mode = true;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyUp) {
                if(app->delete_selection > 0) app->delete_selection--;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyDown) {
                if(app->delete_selection < (app->profile_count - 1)) app->delete_selection++;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                app->delete_select_mode = false;
                app->delete_confirm_mode = true;
                view_port_update(view_port);
            }
        } else if(app->settings_edit_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->settings_edit_mode = false;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyUp) {
                app->settings_edit_value += garage_test_settings_step(app->settings_selection);
                view_port_update(view_port);
            } else if(input_event.key == InputKeyDown) {
                uint32_t step = garage_test_settings_step(app->settings_selection);
                uint32_t min_value = garage_test_settings_min(app->settings_selection);
                if(app->settings_edit_value > (min_value + step - 1)) {
                    app->settings_edit_value -= step;
                } else {
                    app->settings_edit_value = min_value;
                }
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                garage_test_apply_settings_value(app, app->settings_selection, app->settings_edit_value);
                garage_test_save_values(app);
                app->settings_edit_mode = false;
                view_port_update(view_port);
            }
        } else if(app->settings_menu_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->settings_menu_mode = false;
                app->profile_menu_mode = true;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyUp) {
                if(app->settings_selection > 0) app->settings_selection--;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyDown) {
                if(app->settings_selection < 3) app->settings_selection++;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                app->settings_edit_value = garage_test_settings_value(app, app->settings_selection);
                app->settings_edit_mode = true;
                view_port_update(view_port);
            }
        } else if(app->profile_menu_mode) {
            if(input_event.type != InputTypeShort) continue;
            if(input_event.key == InputKeyBack) {
                app->profile_menu_mode = false;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyUp) {
                if(app->profile_menu_selection > 0) app->profile_menu_selection--;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyDown) {
                if(app->profile_menu_selection < 3) app->profile_menu_selection++;
                view_port_update(view_port);
            } else if(input_event.key == InputKeyOk) {
                if(app->profile_menu_selection == 0) {
                    garage_test_copy_string(
                        app->name_edit_buffer,
                        sizeof(app->name_edit_buffer),
                        garage_test_active_profile(app)->name);
                    for(size_t i = strlen(app->name_edit_buffer); i < (GARAGE_TEST_NAME_LEN - 1); i++) {
                        app->name_edit_buffer[i] = ' ';
                    }
                    app->name_edit_buffer[GARAGE_TEST_NAME_LEN - 1] = '\0';
                    app->name_edit_cursor = 0;
                    app->rename_mode = true;
                } else if(app->profile_menu_selection == 1) {
                    FURI_LOG_I(
                        TAG,
                        "Profile Edit -> Edit Settings start (profile_menu=%d settings_menu=%d selection=%u)",
                        app->profile_menu_mode,
                        app->settings_menu_mode,
                        app->settings_selection);
                    app->settings_selection = 0;
                    app->profile_menu_mode = false;
                    app->settings_menu_mode = true;
                    app->settings_edit_mode = false;
                    FURI_LOG_I(
                        TAG,
                        "Profile Edit -> Edit Settings done (profile_menu=%d settings_menu=%d selection=%u)",
                        app->profile_menu_mode,
                        app->settings_menu_mode,
                        app->settings_selection);
                } else if(app->profile_menu_selection == 2) {
                    if(app->profile_count < GARAGE_TEST_MAX_PROFILES) {
                        uint8_t new_index = app->profile_count;
                        char new_name[GARAGE_TEST_NAME_LEN];
                        garage_test_generate_new_bike_name(app, new_name, sizeof(new_name));
                        garage_test_init_profile(
                            &app->profiles[new_index], new_name, 0, 0, 3000, 500, 32, 36);
                        app->active_profile_index = new_index;
                        garage_test_set_profile_count(app, app->profile_count + 1);
                        garage_test_save_values(app);
                        app->profile_menu_mode = false;
                    } else {
                        app->max_bikes_message_mode = true;
                        app->profile_menu_mode = false;
                    }
                } else {
                    app->profile_menu_mode = false;
                    if(app->profile_count == 1) {
                        app->delete_blocked_mode = true;
                    } else {
                        app->delete_selection = app->active_profile_index;
                        app->delete_select_mode = true;
                    }
                }
                view_port_update(view_port);
            }
        } else if(input_event.key == InputKeyBack) {
            if(input_event.type != InputTypeShort) continue;
            if(app->edit_mode) {
                app->edit_mode = false;
                view_port_update(view_port);
            } else {
                running = false;
            }
        } else if(input_event.type != InputTypeShort) {
            continue;
        } else if((app->screen == 0) && (input_event.key == InputKeyDown)) {
            app->profile_menu_selection = 0;
            app->profile_menu_mode = true;
            view_port_update(view_port);
        } else if((app->screen == 0) && (input_event.key == InputKeyOk)) {
            app->active_profile_index = (app->active_profile_index + 1) % app->profile_count;
            garage_test_save_values(app);
            view_port_update(view_port);
        } else if((app->screen == 0) && (input_event.key == InputKeyRight)) {
            app->screen = 1;
            view_port_update(view_port);
        } else if((app->screen == 1) && !app->edit_mode && (input_event.key == InputKeyRight)) {
            app->screen = 2;
            view_port_update(view_port);
        } else if((app->screen == 1) && !app->edit_mode && (input_event.key == InputKeyUp)) {
            if(app->selected_item > 0) {
                app->selected_item--;
                view_port_update(view_port);
            }
        } else if((app->screen == 1) && !app->edit_mode && (input_event.key == InputKeyDown)) {
            if(app->selected_item < 1) {
                app->selected_item++;
                view_port_update(view_port);
            }
        } else if((app->screen == 1) && !app->edit_mode && (input_event.key == InputKeyOk)) {
            app->edit_mode = true;
            app->edit_value = app->selected_item == 0 ? garage_test_active_profile(app)->oil_miles :
                                                        garage_test_active_profile(app)->chain_miles;
            view_port_update(view_port);
        } else if((app->screen == 1) && !app->edit_mode && (input_event.key == InputKeyLeft)) {
            app->screen = 0;
            view_port_update(view_port);
        } else if((app->screen == 2) && (input_event.key == InputKeyLeft)) {
            app->screen = 1;
            view_port_update(view_port);
        } else if((app->screen == 1) && app->edit_mode && (input_event.key == InputKeyUp)) {
            app->edit_value += 100;
            view_port_update(view_port);
        } else if((app->screen == 1) && app->edit_mode && (input_event.key == InputKeyDown)) {
            if(app->edit_value >= 100) {
                app->edit_value -= 100;
            } else {
                app->edit_value = 0;
            }
            view_port_update(view_port);
        } else if((app->screen == 1) && app->edit_mode && (input_event.key == InputKeyOk)) {
            if(app->selected_item == 0) {
                garage_test_active_profile(app)->oil_miles = app->edit_value;
            } else {
                garage_test_active_profile(app)->chain_miles = app->edit_value;
            }
            garage_test_save_values(app);
            app->edit_mode = false;
            view_port_update(view_port);
        } else if((app->screen == 1) && app->edit_mode && (input_event.key == InputKeyRight)) {
            if(app->selected_item == 0) {
                garage_test_active_profile(app)->oil_miles = 0;
                app->oil_last_reset = furi_get_tick();
                app->edit_value = garage_test_active_profile(app)->oil_miles;
            } else {
                garage_test_active_profile(app)->chain_miles = 0;
                app->chain_last_reset = furi_get_tick();
                app->edit_value = garage_test_active_profile(app)->chain_miles;
            }
            garage_test_save_values(app);
            app->edit_mode = false;
            view_port_update(view_port);
        }
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
