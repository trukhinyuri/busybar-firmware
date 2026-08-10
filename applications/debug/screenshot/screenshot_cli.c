/**
 * @file screenshot_cli.c
 * @brief Dump the contents of both displays to storage.
 *
 * Written for debugging on an assembled device: the CLI already lets input be
 * injected, but without a way to see the result the only feedback available is
 * whatever a service happens to log. This closes that loop by writing the raw
 * frame buffers where they can be fetched over the storage API and rendered on
 * a workstation.
 *
 * Unlike the CDC screen service, this does not reconfigure USB, so the virtual
 * ethernet link — and with it the HTTP API and this very CLI — keeps working.
 */
#include <furi.h>

#include <containers/pipe.h>
#include <cli/cli_command.h>
#include <gui/gui.h>
#include <storage/storage.h>

#define TAG "Screenshot"

#define SCREENSHOT_FRONT_PATH EXT_PATH("screen_front.raw")
#define SCREENSHOT_BACK_PATH  EXT_PATH("screen_back.raw")

/**
 * Write one display's frame buffer verbatim.
 *
 * No header is added: the geometry is fixed per display and known to the
 * decoder, and a bare dump keeps the file trivially convertible.
 */
static bool screenshot_write_display(
    PipeSide* pipe,
    Gui* gui,
    Storage* storage,
    GuiDisplayId display_id,
    const char* path) {
    const GuiDisplayParameters* params = gui_display_get_parameters(display_id);

    bool success = false;
    File* file = storage_file_alloc(storage);

    do {
        const uint8_t* frame_buffer = NULL;

        /*
         * Hold the GUI lock only for the copy: writing to eMMC while the lock
         * is held would stall every drawing operation on the device.
         */
        uint8_t* snapshot = malloc(params->buffer_size);
        with_gui(gui, {
            frame_buffer = gui_display_get_frame_buffer(gui, display_id);
            if(frame_buffer) memcpy(snapshot, frame_buffer, params->buffer_size);
        });

        if(!frame_buffer) {
            printf("Error! No frame buffer for display %d\r\n", display_id);
            free(snapshot);
            break;
        }

        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            printf("Error! Cannot open '%s'\r\n", path);
            free(snapshot);
            break;
        }

        const size_t written = storage_file_write(file, snapshot, params->buffer_size);
        free(snapshot);

        if(written != params->buffer_size) {
            printf("Error! Short write to '%s'\r\n", path);
            break;
        }

        printf(
            "%s: %ux%u %ubpp, %u bytes\r\n",
            path,
            (unsigned)params->width,
            (unsigned)params->height,
            (unsigned)params->bits_per_pixel,
            (unsigned)params->buffer_size);

        success = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);

    UNUSED(pipe);
    return success;
}

void screenshot_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);

    Gui* gui = furi_record_open(RECORD_GUI);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    screenshot_write_display(pipe, gui, storage, GuiDisplayIdFront, SCREENSHOT_FRONT_PATH);
    screenshot_write_display(pipe, gui, storage, GuiDisplayIdBack, SCREENSHOT_BACK_PATH);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
}
