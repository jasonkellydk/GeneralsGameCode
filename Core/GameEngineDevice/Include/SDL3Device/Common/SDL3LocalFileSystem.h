#pragma once

#include "Common/LocalFileSystem.h"

#include <filesystem>

class SDL3LocalFileSystem : public LocalFileSystem
{
public:
	SDL3LocalFileSystem();
	~SDL3LocalFileSystem() override;

	void init() override;
	void reset() override;
	void update() override;

	File *openFile(const Char *filename, Int access = File::NONE,
		size_t bufferSize = File::BUFFERSIZE) override;
	Bool doesFileExist(const Char *filename) const override;
	void getFileListInDirectory(const AsciiString &currentDirectory,
		const AsciiString &originalDirectory, const AsciiString &searchName,
		FilenameList &filenameList, Bool searchSubdirectories) const override;
	Bool getFileInfo(const AsciiString &filename, FileInfo *fileInfo) const override;
	Bool createDirectory(AsciiString directory) override;
	AsciiString normalizePath(const AsciiString &filePath) const override;

private:
	std::filesystem::path resolvePath(const Char *filename, Int access) const;

	std::filesystem::path m_basePath;
};
