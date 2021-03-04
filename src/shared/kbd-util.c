/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <ftw.h>

#include "kbd-util.h"
#include "log.h"
#include "nulstr-util.h"
#include "path-util.h"
#include "set.h"
#include "string-util.h"
#include "strv.h"
#include "utf8.h"

static thread_local Set *keymaps = NULL;

static int nftw_cb(
                const char *fpath,
                const struct stat *sb,
                int tflag,
                struct FTW *ftwbuf) {

        _cleanup_free_ char *p = NULL;
        char *e;
        int r;

        if (tflag != FTW_F)
                return 0;

        if (!endswith(fpath, ".map") &&
            !endswith(fpath, ".map.gz"))
                return 0;

        p = strdup(basename(fpath));
        if (!p) {
                errno = ENOMEM;
                return -1;
        }

        e = endswith(p, ".map");
        if (!e)
                e = endswith(p, ".map.gz");
        if (e)
                *e = 0;

        if (!keymap_is_valid(p))
                return 0;

        r = set_consume(keymaps, TAKE_PTR(p));
        if (r < 0 && r != -EEXIST) {
                errno = -r;
                return -1;
        }

        return 0;
}

int get_keymaps(char ***ret) {
        keymaps = set_new(&string_hash_ops);
        if (!keymaps)
                return -ENOMEM;

        const char *dir;
        NULSTR_FOREACH(dir, KBD_KEYMAP_DIRS)
                if (nftw(dir, nftw_cb, 20, FTW_PHYS) < 0) {
                        if (errno != ENOENT)
                                log_debug_errno(errno, "Failed to read keymap list from %s, ignoring: %m", dir);
                }

        _cleanup_strv_free_ char **l = set_get_strv(keymaps);
        if (!l) {
                keymaps = set_free_free(keymaps);
                return -ENOMEM;
        }

        keymaps = set_free(keymaps);

        if (strv_isempty(l))
                return -ENOENT;

        strv_sort(l);

        *ret = TAKE_PTR(l);

        return 0;
}

bool keymap_is_valid(const char *name) {
        if (isempty(name))
                return false;

        if (strlen(name) >= 128)
                return false;

        if (!utf8_is_valid(name))
                return false;

        if (!filename_is_valid(name))
                return false;

        if (!string_is_safe(name))
                return false;

        return true;
}
