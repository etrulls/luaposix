/*
 * POSIX library for Lua 5.1/5.2.
 * (c) Gary V. Vaughan <gary@vaughan.pe>, 2013-2014
 * (c) Reuben Thomas <rrt@sc3d.org> 2010-2013
 * (c) Natanael Copa <natanael.copa@gmail.com> 2008-2010
 * Clean up and bug fixes by Leo Razoumov <slonik.az@gmail.com> 2006-10-11
 * Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br> 07 Apr 2006 23:17:49
 * Based on original by Claudio Terra for Lua 3.x.
 * With contributions by Roberto Ierusalimschy.
 * With documentation from Steve Donovan 2012
 */
/***
 File Control.

 Low-level control over file descriptors, including creating new file
 descriptors with `open`.

@module posix.fcntl
*/

#include <config.h>

#include <fcntl.h>

#include "_helpers.c"


/* Darwin fails to define O_RSYNC. */
#ifndef O_RSYNC
#define O_RSYNC 0
#endif
/* FreeBSD 10 fails to define O_DSYNC. */
#ifndef O_DSYNC
#define O_DSYNC 0
#endif



#if HAVE_POSIX_FADVISE
/***
Instruct kernel on appropriate cache behaviour for a file or file segment.
@function fadvise
@param file Lua file object
@int offset start of region
@int len number of bytes in region
@int advice one of `POSIX_FADV\_NORMAL`, `POSIX_FADV\_SEQUENTIAL`,
  `POSIX_FADV\_RANDOM`, `POSIX_FADV_\NOREUSE`, `POSIX_FADV\_WILLNEED` or
  `POSIX_FADV\_DONTNEED`
@treturn[1] int `0`, if successful
@return[2] nil
@treturn[2] string error message
@see posix_fadvise(2)
*/
/* FIXME: a minimal interface takes a file descriptor, not a handle! */
static int
Pfadvise(lua_State *L)
{
	FILE *f = *(FILE**) luaL_checkudata(L, 1, LUA_FILEHANDLE);
	const lua_Integer offset = checkint(L, 2);
	const lua_Integer len    = checkint(L, 3);
	const lua_Integer advice = checkint(L, 4);
	int res;
	checknargs(L, 4);
	res = posix_fadvise(fileno (f), offset, len, advice);
	return pushresult(L, res == 0 ? 0 : -1, "posix_fadvise");
}
#endif

/***
Manipulate file descriptor.
@function fcntl
@int fd file descriptor to act on
@int cmd operation to perform
@tparam[opt=0] int|flock arg when *cmd* is `F_GETLK`, `F_SETLK` or `F_SETLKW`,
  then *arg* is a @{flock} table, otherwise an integer with meaning dependent
  upon the value of *cmd*.
@return[1] integer return value depending on *cmd*, if successful
@return[2] nil
@treturn[2] string error message
@see fcntl(2)
@see lock.lua
@usage
local flag = P.fcntl (fd, P.F_GETFL)
*/
static int
Pfcntl(lua_State *L)
{
	int fd = checkint(L, 1);
	int cmd = checkint(L, 2);
	int arg;
	struct flock lockinfo;
	int r;
	checknargs(L, 3);
	switch (cmd)
	{
		case F_SETLK:
		case F_SETLKW:
		case F_GETLK:
			luaL_checktype(L, 3, LUA_TTABLE);

			/* Copy fields to flock struct */
			lua_getfield(L, 3, "l_type");
			lockinfo.l_type = (short)lua_tointeger(L, -1);
			lua_getfield(L, 3, "l_whence");
			lockinfo.l_whence = (short)lua_tointeger(L, -1);
			lua_getfield(L, 3, "l_start");
			lockinfo.l_start = (off_t)lua_tointeger(L, -1);
			lua_getfield(L, 3, "l_len");
			lockinfo.l_len = (off_t)lua_tointeger(L, -1);

			/* Lock */
			r = fcntl(fd, cmd, &lockinfo);

			/* Copy fields from flock struct */
			lua_pushinteger(L, lockinfo.l_type);
			lua_setfield(L, 3, "l_type");
			lua_pushinteger(L, lockinfo.l_whence);
			lua_setfield(L, 3, "l_whence");
			lua_pushinteger(L, lockinfo.l_start);
			lua_setfield(L, 3, "l_start");
			lua_pushinteger(L, lockinfo.l_len);
			lua_setfield(L, 3, "l_len");
			lua_pushinteger(L, lockinfo.l_pid);
			lua_setfield(L, 3, "l_pid");

			break;
		default:
			arg = optint(L, 3, 0);
			r = fcntl(fd, cmd, arg);
			break;
	}
	return pushresult(L, r, "fcntl");
}


/***
Open a file.
@function open
@string path
@int oflags bitwise OR of zero or more of `O_RDONLY`, `O_WRONLY`, `O_RDWR`,
  `O_APPEND`, `O_CREAT`, `O_DSYNC`, `O_EXCL`, `O_NOCTTY`, `O_NONBLOCK`,
  `O_RSYNC`, `O_SYNC`, `O_TRUNC`
@string mode (used with `O_CREAT`; see @{posix.sys.stat.chmod} for format)
@treturn[1] int file descriptor for *path*, if successful
@return[2] nil
@treturn[2] string error message
@see open(2)
@usage
fd = P.open ("data", bit.bor (P.O_CREAT, P.O_RDWR), "rw-r-----")
*/
static int
Popen(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int flags = checkint(L, 2);
	mode_t mode = (mode_t) 0;
	if (flags & O_CREAT)
	{
		checknargs(L, 3);
		const char *modestr = luaL_checkstring(L, 3);
		if (mode_munch(&mode, modestr))
			luaL_argerror(L, 3, "bad mode");
	}
	else
		checknargs(L, 2);
	return pushresult(L, open(path, flags, mode), path);
}


static const luaL_Reg posix_fcntl_fns[] =
{
#if HAVE_POSIX_FADVISE
	LPOSIX_FUNC( Pfadvise		),
#endif
	LPOSIX_FUNC( Pfcntl		),
	LPOSIX_FUNC( Popen		),
	{NULL, NULL}
};


LUALIB_API int
luaopen_posix_fcntl(lua_State *L)
{
	luaL_register(L, "posix.fcntl", posix_fcntl_fns);
	lua_pushliteral(L, "posix.fcntl for " LUA_VERSION " / " PACKAGE_STRING);
	lua_setfield(L, -2, "version");

	/* fcntl flags */
	LPOSIX_CONST( F_DUPFD		);
	LPOSIX_CONST( F_GETFD		);
	LPOSIX_CONST( F_SETFD		);
	LPOSIX_CONST( F_GETFL		);
	LPOSIX_CONST( F_SETFL		);
	LPOSIX_CONST( F_GETLK		);
	LPOSIX_CONST( F_SETLK		);
	LPOSIX_CONST( F_SETLKW		);
	LPOSIX_CONST( F_GETOWN		);
	LPOSIX_CONST( F_SETOWN		);
	LPOSIX_CONST( F_RDLCK		);
	LPOSIX_CONST( F_WRLCK		);
	LPOSIX_CONST( F_UNLCK		);

	/* file creation & status flags */
	LPOSIX_CONST( O_RDONLY		);
	LPOSIX_CONST( O_WRONLY		);
	LPOSIX_CONST( O_RDWR		);
	LPOSIX_CONST( O_APPEND		);
	LPOSIX_CONST( O_CREAT		);
	LPOSIX_CONST( O_DSYNC		);
	LPOSIX_CONST( O_EXCL		);
	LPOSIX_CONST( O_NOCTTY		);
	LPOSIX_CONST( O_NONBLOCK	);
	LPOSIX_CONST( O_RSYNC		);
	LPOSIX_CONST( O_SYNC		);
	LPOSIX_CONST( O_TRUNC		);

	/* posix_fadvise flags */
#ifdef POSIX_FADV_NORMAL
	LPOSIX_CONST( POSIX_FADV_NORMAL		);
#endif
#ifdef POSIX_FADV_SEQUENTIAL
	LPOSIX_CONST( POSIX_FADV_SEQUENTIAL	);
#endif
#ifdef POSIX_FADV_RANDOM
	LPOSIX_CONST( POSIX_FADV_RANDOM		);
#endif
#ifdef POSIX_FADV_NOREUSE
	LPOSIX_CONST( POSIX_FADV_NOREUSE	);
#endif
#ifdef POSIX_FADV_WILLNEED
	LPOSIX_CONST( POSIX_FADV_WILLNEED	);
#endif
#ifdef POSIX_FADV_DONTNEED
	LPOSIX_CONST( POSIX_FADV_DONTNEED	);
#endif

	return 1;
}


/***
Tables.
@section tables
*/

/***
Advisory file locks.
Passed as *arg* to @{fcntl} when *cmd* is `F_GETLK`, `F_SETLK` or `F_SETLKW`.
@table flock
@int l_start starting offset
@int l_len len = 0 means until end of file
@int l_pid lock owner
@int l_type lock type
@int l_whence one of `SEEK_SET`, `SEEK_CUR` or `SEEK_END`
*/