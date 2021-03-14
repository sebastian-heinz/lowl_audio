#ifdef LOWL_WIN

#include "lowl_file.h"

struct LowlFileWin {
	FILE* file;
};

std::string Lowl::LowlFile::get_path()
{
	return path;
}

void Lowl::LowlFile::open(const std::string& p_path, Error& error)
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	path = p_path;

	struct _stat st;
	if (_stat(path.c_str(), &st) == 0) {
		if (!(st.st_mode & _S_IFREG))
			return;
	};

	errno_t errcode = fopen_s(&win->file, path.c_str(), "rb");

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

void Lowl::LowlFile::close()
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return;
	}
	fclose(win->file);
	win->file = NULL;
}

void Lowl::LowlFile::seek(size_t p_position)
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

size_t Lowl::LowlFile::get_position() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	size_t aux_position = 0;
	aux_position = ftell(win->file);
	if (!aux_position) {

	};
	return aux_position;
}

size_t Lowl::LowlFile::get_length() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return 0;
	}

	//size_t pos = get_position();
	//fseek(win->file, 0, SEEK_END);
	//int size = get_position();
	//fseek(win->file, pos, SEEK_SET);


	long pos = ftell(win->file);
	if (pos < 0) {
		// error
	}
	int seek_end = fseek(win->file, 0, SEEK_END);
	if (!seek_end) {
		// error
	}
	long size = ftell(win->file);
	if (size < 0) {
		// error
	}
	int seek_set = fseek(win->file, pos, SEEK_SET);
	if (!seek_set) {
		// error
	}
	return size;
}

uint8_t Lowl::LowlFile::read_u8() const
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

std::unique_ptr<uint8_t[]> Lowl::LowlFile::read_buffer(size_t& length) const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file) {
		// error
		return 0;
	}
	std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(length);
	length = fread(data.get(), 1, length, win->file);
	return data;
}

bool Lowl::LowlFile::is_eof() const
{
	LowlFileWin* win = (LowlFileWin*)user_data;
	if (!win->file)
	{
		return true;
	}
	return feof(win->file);
}

Lowl::LowlFile::LowlFile()
{
	user_data = new LowlFileWin;
	LowlFileWin* win = (LowlFileWin*)user_data;
	win->file = nullptr;
	path = std::string();
}

Lowl::LowlFile::~LowlFile()
{
	delete user_data;
}

#endif