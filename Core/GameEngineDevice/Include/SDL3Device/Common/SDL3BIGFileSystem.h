#pragma once

#include "Common/ArchiveFileSystem.h"

class SDL3BIGFileSystem : public ArchiveFileSystem
{
public:
	SDL3BIGFileSystem();
	~SDL3BIGFileSystem() override;

	void init() override;
	void update() override;
	void reset() override;
	void postProcessLoad() override;

	void closeAllArchiveFiles() override;
	ArchiveFile *openArchiveFile(const Char *filename) override;
	void closeArchiveFile(const Char *filename) override;
	void closeAllFiles() override;
	Bool loadBigFilesFromDirectory(AsciiString dir, AsciiString fileMask,
		Bool overwrite = FALSE) override;
};
