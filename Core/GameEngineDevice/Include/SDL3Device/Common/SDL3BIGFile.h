#pragma once

#include "Common/ArchiveFile.h"

class SDL3BIGFile : public ArchiveFile
{
public:
	SDL3BIGFile(AsciiString name, AsciiString path);
	~SDL3BIGFile() override;

	Bool getFileInfo(const AsciiString &filename, FileInfo *fileInfo) const override;
	File *openFile(const Char *filename, Int access = 0) override;
	void closeAllFiles() override;
	AsciiString getName() override;
	AsciiString getPath() override;
	void setSearchPriority(Int newPriority) override;
	void close() override;

protected:
	AsciiString m_name;
	AsciiString m_path;
};
