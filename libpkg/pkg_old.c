/*-
 * Copyright (c) 2012 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <regex.h>

#include <pkg.h>
#include <private/pkg.h>

static const char * const scripts[] = {
	"+INSTALL",
	"+PRE_INSTALL",
	"+POST_INSTALL",
	"+POST_INSTALL",
	"+DEINSTALL",
	"+PRE_DEINSTALL",
	"+POST_DEINSTALL",
	"+UPGRADE",
	"+PRE_UPGRADE",
	"+POST_UPGRADE",
	"pkg-install",
	"pkg-pre-install",
	"pkg-post-install",
	"pkg-deinstall",
	"pkg-pre-deinstall",
	"pkg-post-deinstall",
	"pkg-upgrade",
	"pkg-pre-upgrade",
	"pkg-post-upgrade",
	NULL
};

int
pkg_old_load_from_path(struct pkg *pkg, const char *path)
{
	char *desc;
	char *www;
	char fpath[MAXPATHLEN];
	regex_t preg;
	regmatch_t pmatch[2];
	int i;
	size_t size;

	if (!is_dir(path))
		return (EPKG_FATAL);

	snprintf(fpath, MAXPATHLEN, "%s/+CONTENTS", path);
	if (ports_parse_plist(pkg, fpath, NULL) != EPKG_OK)
		return (EPKG_FATAL);

	snprintf(fpath, MAXPATHLEN, "%s/+COMMENT", path);
	if (access(fpath, F_OK) == 0)
		pkg_set_from_file(pkg, PKG_COMMENT, fpath);

	snprintf(fpath, sizeof(fpath), "%s/+DESC", path);
	if (access(fpath, F_OK) == 0)
		pkg_set_from_file(pkg, PKG_DESC, fpath);

	snprintf(fpath, sizeof(fpath), "%s/+DISPLAY", path);
	if (access(fpath, F_OK) == 0)
		pkg_set_from_file(pkg, PKG_MESSAGE, fpath);

	snprintf(fpath, sizeof(fpath), "%s/+MTREE_DIRS", path);
	if (access(fpath, F_OK) == 0)
		pkg_set_from_file(pkg, PKG_MTREE, fpath);

	for (i = 0; scripts[i] != NULL; i++) {
		snprintf(fpath, sizeof(fpath), "%s/%s", path, scripts[i]);
		if (access(fpath, F_OK) == 0)
			pkg_addscript_file(pkg, fpath);
	}

	pkg_get(pkg, PKG_DESC, &desc);
	regcomp(&preg, "^WWW:[[:space:]]*(.*)$", REG_EXTENDED|REG_ICASE|REG_NEWLINE);
	if (regexec(&preg, desc, 2, pmatch, 0) == 0) {
		size = pmatch[1].rm_eo - pmatch[1].rm_so;
		www = strndup(&desc[pmatch[1].rm_so], size);
		pkg_set(pkg, PKG_WWW, www);
		free(www);
	} else {
		pkg_set(pkg, PKG_WWW, "UNKNOWN");
	}
	regfree(&preg);

	return (EPKG_OK);
}

int
pkg_old_emit_content(struct pkg *pkg, char **dest)
{
	struct sbuf *content = sbuf_new_auto();

	struct pkg_dep *dep = NULL;
	struct pkg_file *file = NULL;
	struct pkg_dir *dir = NULL;

	const char *name;
	const char *pkgorigin, *prefix, *version;

	pkg_get(pkg, PKG_NAME, &name, PKG_ORIGIN, &pkgorigin,
	    PKG_PREFIX, &prefix, PKG_VERSION, &version);

	sbuf_printf(content,
	    "@comment PKG_FORMAT_REVISION:1.1\n"
	    "@name %s-%s\n"
	    "@comment ORIGIN:%s\n"
	    "@cwd %s\n"
	    /* hack because we can recreate the prefix split or origin */
	    "@cwd /\n",
	    name, version, pkgorigin, prefix);

	while (pkg_deps(pkg, &dep) == EPKG_OK) {
		sbuf_printf(content,
		    "@pkgdep %s-%s\n"
		    "@comment DEPORIGIN:%s\n",
		    pkg_dep_name(dep),
		    pkg_dep_version(dep),
		    pkg_dep_origin(dep));
	}

	while (pkg_files(pkg, &file) == EPKG_OK) {
		sbuf_printf(content,
		    "%s\n"
		    "@comment MD5:%s\n",
		     pkg_file_path(file) + 1,
		     pkg_file_cksum(file));
	}

	while (pkg_dirs(pkg, &dir)) {
		if (pkg_dir_try(dir)) {
			sbuf_printf(content,
			    "@dirrm %s\n",
			    pkg_dir_path(dir));
		} else {
			sbuf_printf(content,
			    "@unexec /sbin/rmdir \"%s\" 2>/dev/null\n",
			    pkg_dir_path(dir));
		}
	}

	sbuf_finish(content);
	*dest = strdup(sbuf_get(content));
	sbuf_delete(content);

	return (EPKG_OK);
}