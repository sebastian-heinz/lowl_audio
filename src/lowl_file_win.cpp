#ifdef LOWL_WIN

#include "lowl_file.h"

struct LowlFileWin {
	FILE* file;
};

std::wstring LowlFile::get_path()
{
	return path;
}

void LowlFile::open(const std::wstring& p_path)
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	path = p_path;

	struct _stat st;
	if (_wstat(path.c_str(), &st) == 0) {
		if (!(st.st_mode & _S_IFREG))
			return;
	};

	errno_t errcode = _wfopen_s(&win->file, path.c_str(), L"rb");

	if (win->file == NULL) {
		switch (errcode) {
		case ENOENT: {
			//last_error = ERR_FILE_NOT_FOUND;	
			return;
		} break;
		default: {
			//last_error = ERR_FILE_CANT_OPEN;
			return;
		} break;
		}
		return;
	}
	else {
		//last_error = OK;
		//flags = p_mode_flags;
		//return OK;
	}
	return;
}

void LowlFile::close()
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return;
	}
	fclose(win->file);
	win->file = NULL;
}

void LowlFile::seek(size_t p_position)
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return;
	}
	if (fseek(win->file, p_position, SEEK_SET))
	{

	}

}

size_t LowlFile::get_position() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	size_t aux_position = 0;
	aux_position = ftell(win->file);
	if (!aux_position) {

	};
	return aux_position;
}

size_t LowlFile::get_length() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return 0;
	}

	size_t pos = get_position();
	fseek(win->file, 0, SEEK_END);
	int size = get_position();
	fseek(win->file, pos, SEEK_SET);

	return size;
}

uint8_t LowlFile::read_u8() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return 0;
	}
	uint8_t b;
	if (fread(&b, 1, 1, win->file) == 0) {
		b = '\0';
	};
	return b;
}

int LowlFile::get_buffer(uint8_t* p_dst, int p_length) const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return -1;
	}
	int read = fread(p_dst, 1, p_length, win->file);
	return read;
}

bool LowlFile::is_eof() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return true;
	}
	return feof(win->file);
}

LowlFile::LowlFile()
{
	user_data = new LowlFileWin;
	path = std::wstring();
}

LowlFile::~LowlFile()
{
	delete user_data;
}

#endif