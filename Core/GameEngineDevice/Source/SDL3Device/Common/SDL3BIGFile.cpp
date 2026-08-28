#include "Common/LocalFileSystem.h"
#include "Common/RAMFile.h"
#include "Common/StreamingArchiveFile.h"
#include "Common/GameMemory.h"
#include "SDL3Device/Common/SDL3BIGFile.h"

SDL3BIGFile::SDL3BIGFile(AsciiString name, AsciiString path)
	: m_name(name)
	, m_path(path)
{
}

SDL3BIGFile::~SDL3BIGFile() = default;

File *SDL3BIGFile::openFile(const Char *filename, Int access)
{
	const ArchivedFileInfo *fileInfo = getArchivedFileInfo(AsciiString(filename));
	if (fileInfo == nullptr)
		return nullptr;

	RAMFile *ramFile = BitIsSet(access, File::STREAMING)
		? static_cast<RAMFile *>(newInstance(StreamingArchiveFile))
		: static_cast<RAMFile *>(newInstance(RAMFile));

	ramFile->deleteOnClose();
	if (!ramFile->openFromArchive(m_file, fileInfo->m_filename, fileInfo->m_offset, fileInfo->m_size))
	{
		ramFile->close();
		return nullptr;
	}

	if ((access & File::WRITE) == 0)
		return ramFile;

	if (TheLocalFileSystem == nullptr)
	{
		ramFile->close();
		return nullptr;
	}

	File *localFile = TheLocalFileSystem->openFile(filename, access, 0);
	if (localFile != nullptr)
		ramFile->copyDataToFile(localFile);

	ramFile->close();
	return localFile;
}

void SDL3BIGFile::closeAllFiles()
{
}

AsciiString SDL3BIGFile::getName()
{
	return m_name;
}

AsciiString SDL3BIGFile::getPath()
{
	return m_path;
}

void SDL3BIGFile::setSearchPriority(Int)
{
}

void SDL3BIGFile::close()
{
}

Bool SDL3BIGFile::getFileInfo(const AsciiString &filename, FileInfo *fileInfo) const
{
	if (fileInfo == nullptr)
		return FALSE;

	const ArchivedFileInfo *archivedFileInfo = getArchivedFileInfo(filename);
	if (archivedFileInfo == nullptr || TheLocalFileSystem == nullptr || m_file == nullptr)
		return FALSE;

	if (!TheLocalFileSystem->getFileInfo(AsciiString(m_file->getName()), fileInfo))
		return FALSE;

	fileInfo->sizeHigh = 0;
	fileInfo->sizeLow = static_cast<Int>(archivedFileInfo->m_size);
	return TRUE;
}
