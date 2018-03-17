#include <conio.h>
#include <direct.h>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <Shlwapi.h>
#include <Windows.h>
#include "libimobiledevice\include.h"
#include "ziputils\unzip.h"

typedef idevice_error_t(*iDevice_new)(idevice_t *, const char *);

typedef afc_error_t(*newAFCClient)(idevice_t, lockdownd_service_descriptor_t, afc_client_t *);
typedef afc_error_t(*openFileAFC)(afc_client_t, const char *, afc_file_mode_t, uint64_t *);
typedef afc_error_t(*readFileAFC)(afc_client_t, uint64_t, char *, uint32_t, uint32_t *);
typedef afc_error_t(*getFileInfoAFC)(afc_client_t, const char *, char ***);
typedef afc_error_t(*writeFileAFC)(afc_client_t, uint64_t, const char *, uint32_t, uint32_t *);
typedef afc_error_t(*closeFileAFC)(afc_client_t, uint64_t);
typedef afc_error_t(*truncateFileAFC)(afc_client_t, uint64_t, uint64_t);
typedef afc_error_t(*readDirectoryAFC)(afc_client_t, const char *, char ***);

typedef lockdownd_error_t(*lockdownClientNewWithHandshake)(idevice_t, lockdownd_client_t *, const char *);
typedef lockdownd_error_t(*lockdowndStartService)(lockdownd_client_t, const char *, lockdownd_service_descriptor_t *);

void setupDevice(HMODULE, idevice_t *, const char *);
void setupLockdowndClient(HMODULE, idevice_t, lockdownd_client_t *, const char *);
void startLockdowndService(HMODULE, lockdownd_client_t, const char *, lockdownd_service_descriptor_t *);
void setupAFCClient(HMODULE, idevice_t, lockdownd_service_descriptor_t, afc_client_t *);
void setupAFCClientFromHouseArrest(HMODULE, house_arrest_client_t, afc_client_t *);

void openFile(HMODULE, afc_client_t, const char *, afc_file_mode_t, uint64_t *);
void getFileInfo(HMODULE, afc_client_t, const char *, char ***);
void readFile(HMODULE, afc_client_t, uint64_t, char *, uint32_t);
void writeFile(HMODULE, afc_client_t, uint64_t, const char *, uint32_t, uint32_t *);
void closeFile(HMODULE, afc_client_t, uint64_t);
void truncateFile(HMODULE, afc_client_t, uint64_t, uint64_t);
void readDirectory(HMODULE, afc_client_t, const char *, char ***);

std::wstring str_to_widestr(const std::string&);

void killProgram();

int main(){
	HMODULE libimobiledeviceDLL = LoadLibrary(L"libimobiledevice.dll");

	if(libimobiledeviceDLL != NULL){
		afc_client_t client;
		idevice_t device;
		lockdownd_client_t lockdowndClient;
		lockdownd_service_descriptor_t lockdowndServiceDescriptor = new lockdownd_service_descriptor();
		
		std::cout << "Please unlock and plug in your iPod. Press any key to continue." << std::endl;

		_getch();

		setupDevice(libimobiledeviceDLL, &device, /*My iPod's UDID*/); //iPod
		setupLockdowndClient(libimobiledeviceDLL, device, &lockdowndClient, "GameTool");
		startLockdowndService(libimobiledeviceDLL, lockdowndClient, "com.apple.afc2", &lockdowndServiceDescriptor);
		setupAFCClient(libimobiledeviceDLL, device, lockdowndServiceDescriptor, &client);

		std::cout << "iPod connected to port " << lockdowndServiceDescriptor->port << "." << std::endl;

		//actual executable name
		std::string executableName;

		std::string binaryName;
		std::string folderName;
		std::string version;

		std::cout << "Cracked IPA name: ";
		getline(std::cin, binaryName);

		//so strstr doesn't give a non-null result if the executable name is present in another file name
		binaryName += "-v";

		std::cout << "Folder name: ";
		getline(std::cin, folderName);

		std::cout << "Version: ";
		getline(std::cin, version);

		//scan directory for its contents
		char **directoryContents = NULL;
		const char *filePath = "/var/mobile/Documents/Cracked/";

		readDirectory(libimobiledeviceDLL, client, filePath, &directoryContents);

		int directoryIndex = 0;
		char *fileName = NULL;

		//find file name in directoryContents array
		do{
			fileName = strstr(directoryContents[directoryIndex], binaryName.c_str());

			directoryIndex++;
		}while(fileName == NULL && directoryContents[directoryIndex] != NULL);

		if(fileName == NULL){
			//substr to exclude the '-v'
			std::cout << binaryName.substr(0, binaryName.length() - 2) << " wasn't found in " << filePath << ". Did you forget to crack the game?" << std::endl;

			killProgram();
		}

		std::cout << "File name: " << fileName << std::endl;

		//construct the path to the ipa on my device
		std::string finalFilePath = std::string(filePath) + std::string(fileName);

		std::cout << "File path: " << finalFilePath << std::endl;

		uint64_t fileHandle;

		//open the file on my device to get info about it
		openFile(libimobiledeviceDLL, client, finalFilePath.c_str(), afc_file_mode_t::AFC_FOPEN_RDONLY, &fileHandle);

		//this will have a length of 12 once initialized, put = NULL also?
		char **fileInfo;

		getFileInfo(libimobiledeviceDLL, client, finalFilePath.c_str(), &fileInfo);

		//size (in bytes) is located at index 1
		const int fileSize = atoi(fileInfo[1]);

		std::cout << "This file is " << fileSize << " bytes." << std::endl;

		//create a buffer with the same size as the ipa on my device
		char *fileBuffer = (char *)malloc(fileSize);

		//put the ipa's contents into fileBuffer
		readFile(libimobiledeviceDLL, client, fileHandle, fileBuffer, fileSize);

		closeFile(libimobiledeviceDLL, client, fileHandle);

		//chop off the "-v"
		binaryName = binaryName.substr(0, binaryName.length() - 2);

		//get where the executable should be on my computer
		std::string fullLocalPath = "C:\\Users\\Justin\\Desktop\\Binaries\\" + folderName + "\\";

		std::wstring fullLocalPathWide = str_to_widestr(fullLocalPath);

		DWORD localDirectoryAttributes = GetFileAttributes(fullLocalPathWide.c_str());

		//create a non-wide string copy of where the executable should be on my computer
		std::string ipaPath = std::string(fullLocalPath.c_str());

		//this directory doesn't exist, in other words this game hasn't been touched by me yet
		if(!PathIsDirectory(fullLocalPathWide.c_str())){
			std::wcout << fullLocalPathWide << " is not a directory. Creating game folder..." << std::endl;

			int makeDirectoryError = mkdir(ipaPath.c_str());

			if(makeDirectoryError != 0){
				std::cout << "Could not make game folder. errno = " << errno << std::endl;

				killProgram();
			}
			else{
				std::cout << ipaPath << " created successfully. Creating version folder..." << std::endl;
			}
		}

		ipaPath += version + "\\";

		int makeDirectoryError = mkdir(ipaPath.c_str());

		if(makeDirectoryError == 0){
			std::cout << "Version folder created successfully." << std::endl;

			//append the actual ipa's file name onto the path for fopen
			std::string ipaPathWithIPA = ipaPath + fileName;

			FILE *ipa = fopen(ipaPathWithIPA.c_str(), "wb");

			if(ipa != NULL){
				std::cout << "IPA created successfully at " << ipaPathWithIPA << std::endl;
			}
			else{
				std::cout << "IPA could not be created." << std::endl;

				killProgram();
			}

			//write data from fileBuffer into the IPA that was just created
			fwrite(fileBuffer, sizeof(char), fileSize, ipa);
			fclose(ipa);

			//change original extension from '.ipa' to '.zip'
			std::string ipaPathRenamedWithZipExtension = ipaPathWithIPA.substr(0, ipaPathWithIPA.length() - 4);
			ipaPathRenamedWithZipExtension += ".zip";

			int renameError = rename(ipaPathWithIPA.c_str(), ipaPathRenamedWithZipExtension.c_str());

			if(renameError == 0){
				std::cout << "IPA renamed successfully." << std::endl;
			}
			else{
				std::cout << "Could not rename IPA." << std::endl;

				killProgram();
			}

			std::cout << "Extracting executable..." << std::endl;

			//so files get unzipped in the right place
			SetCurrentDirectory(str_to_widestr(ipaPath).c_str());

			TCHAR *tcharIPAPath = new TCHAR[ipaPathRenamedWithZipExtension.size() + 1];
			tcharIPAPath[ipaPathRenamedWithZipExtension.size()] = 0;

			std::copy(ipaPathRenamedWithZipExtension.begin(), ipaPathRenamedWithZipExtension.end(), tcharIPAPath);

			HZIP zipWithExecutable = OpenZip(tcharIPAPath, NULL);
			ZIPENTRY zipEntry;

			GetZipItem(zipWithExecutable, -1, &zipEntry);

			int numItems = zipEntry.index;

			//use with substr to get the executable name
			int appExtensionIndex = -1;

			std::wstring wideExecutableName;

			for(int i = 0; i < numItems; i++){
				GetZipItem(zipWithExecutable, i, &zipEntry);
				std::wstring currentNameInArchive = zipEntry.name;

				appExtensionIndex = currentNameInArchive.find(L".app");

				if(appExtensionIndex != -1){
					//only extract the executable
					UnzipItem(zipWithExecutable, i, zipEntry.name);

					//start at 8 to get past '/Payload/'
					wideExecutableName = currentNameInArchive.substr(8, appExtensionIndex - 8);

					///make this more efficient
					break;
				}
			}

			if(appExtensionIndex == -1){
				std::cout << "No executable found???" << std::endl;

				killProgram();
			}

			CloseZip(zipWithExecutable);

			std::cout << "Executable successfully extracted." << std::endl;

			//make a non wide copy of the executable name
			executableName = std::string(wideExecutableName.begin(), wideExecutableName.end());

			std::cout << "Executable name: " << executableName << std::endl;

			//move the executable from /Payload/*.app/ to the game folder
			rename(std::string(ipaPath + "Payload\\" + executableName + ".app\\" + executableName).c_str(), std::string(ipaPath + executableName).c_str());

			//delete the empty executableName.app folder
			rmdir(std::string(ipaPath + "Payload\\" + executableName + ".app\\").c_str());

			//delete the empty Payload folder
			rmdir(std::string(ipaPath + "Payload").c_str());

			//delete the archive containing the executable
			remove(std::string(ipaPathRenamedWithZipExtension).c_str());
		}
		else{
			if(errno == EEXIST){
				std::cout << "This version already exists." << std::endl;

				killProgram();
			}

			std::cout << "Could not make version folder." << std::endl;

			killProgram();
		}
		
		std::cout << "Please unplug your iPod. Unlock and plug in your phone. Press any key to continue." << std::endl;

		_getch();

		//now we upload the 32 bit executable to my 64 bit phone
		//but first we have to reinitialize everything for my phone
		setupDevice(libimobiledeviceDLL, &device, /*My iPhone's UDID*/); //iPhone
		setupLockdowndClient(libimobiledeviceDLL, device, &lockdowndClient, "GameTool");
		startLockdowndService(libimobiledeviceDLL, lockdowndClient, "com.apple.afc2", &lockdowndServiceDescriptor);
		setupAFCClient(libimobiledeviceDLL, device, lockdowndServiceDescriptor, &client);

		std::cout << "iPhone connected to port " << lockdowndServiceDescriptor->port << "." << std::endl;

		std::string executableToUpload = ipaPath + executableName;

		FILE *executable = fopen(executableToUpload.c_str(), "rb");

		if(executable != NULL){
			char *localFileToUploadBuffer = (char *)malloc(fileSize);

			fread((void *)localFileToUploadBuffer, fileSize, 1, executable);
			fclose(executable);

			//scan the Applications folder for the right place to put the executable
			char **directoryContents = NULL;
			std::string filePath = "/var/mobile/Containers/Bundle/Application/";

			readDirectory(libimobiledeviceDLL, client, filePath.c_str(), &directoryContents);

			//start at two so we don't scan . and ..
			int currentFolderIndex = 2;
			std::string folderToUploadIn = filePath;

			//look inside every folder for the one with binaryName.app
			while(directoryContents[currentFolderIndex] != NULL){
				filePath += directoryContents[currentFolderIndex];

				char **appFiles = NULL;

				//skip . and .. in this current directory
				int currentAppFileIndex = 2;

				readDirectory(libimobiledeviceDLL, client, filePath.c_str(), &appFiles);

				//scan inside directories from directoryContents
				while(appFiles[currentAppFileIndex] != NULL){
					if(strcmp(appFiles[currentAppFileIndex], (executableName + ".app").c_str()) == 0){
						folderToUploadIn = filePath + "/" + std::string(appFiles[currentAppFileIndex]);

						break;
					}

					currentAppFileIndex++;
				}

				filePath = "/var/mobile/Containers/Bundle/Application/";
				currentFolderIndex++;
			}

			std::cout << "Will upload to " << folderToUploadIn << std::endl;
			
			uint32_t writtenBytes;

			std::cout << "Uploading..." << std::endl;

			openFile(libimobiledeviceDLL, client, std::string(folderToUploadIn + "/" + executableName).c_str(), afc_file_mode_t::AFC_FOPEN_RW, &fileHandle);
			writeFile(libimobiledeviceDLL, client, fileHandle, localFileToUploadBuffer, fileSize, &writtenBytes);
			truncateFile(libimobiledeviceDLL, client, fileHandle, fileSize); //resize file after writing to it

			closeFile(libimobiledeviceDLL, client, fileHandle);

			std::cout << "Done uploading." << std::endl;
			
			std::cout << "Success! You're good to remove ASLR and run bintool." << std::endl;
		}
		else{
			std::cout << "Could not open " + executableToUpload << std::endl;
		}

		_getch();

		return 0;
	}
	else{
		std::cout << "Error loading libimobiledevice DLL. GetLastError=" << GetLastError() << std::endl;

		killProgram();
	}

	_getch();

	return 0;
}

void setupDevice(HMODULE dll, idevice_t *device, const char *udid){
	iDevice_new iDevice_new_func = (iDevice_new)GetProcAddress(dll, "idevice_new");

	if(iDevice_new_func != NULL){
		idevice_error_t result = iDevice_new_func(device, udid);

		if(result == 0){
			std::cout << "Success in getting idevice_new. Result: " << result << std::endl;
		}
		else if(result == idevice_error_t::IDEVICE_E_NO_DEVICE){
			std::cout << "No device is connected, or wrong device is connected." << std::endl;

			killProgram();
		}
		else{
			std::cout << "idevice_new failed. Result: " << result << std::endl;

			killProgram();
		}
	}
	else{
		std::cout << "Error in getting idevice_new." << std::endl;

		killProgram();
	}
}

void setupLockdowndClient(HMODULE dll, idevice_t device, lockdownd_client_t *client, const char *label){
	lockdownClientNewWithHandshake lockdownClientNewWithHandshake_func = (lockdownClientNewWithHandshake)GetProcAddress(dll, "lockdownd_client_new_with_handshake");

	if(lockdownClientNewWithHandshake_func != NULL){
		lockdownd_error_t result = lockdownClientNewWithHandshake_func(device, client, label);

		std::cout << "Success in getting lockdownd_client_new_with_handshake. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting lockdownd_client_new_with_handshake." << std::endl;

		killProgram();
	}
}

void startLockdowndService(HMODULE dll, lockdownd_client_t client, const char *identifier, lockdownd_service_descriptor_t *service){
	lockdowndStartService lockdowndStartService_func = (lockdowndStartService)GetProcAddress(dll, "lockdownd_start_service");

	if(lockdowndStartService_func != NULL){
		lockdownd_error_t result = lockdowndStartService_func(client, identifier, service);

		std::cout << "Success in getting lockdownd_start_service. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting lockdownd_start_service." << std::endl;

		killProgram();
	}
}

void setupAFCClient(HMODULE dll, idevice_t device, lockdownd_service_descriptor_t service, afc_client_t *client){
	newAFCClient afc_client_new_func = (newAFCClient)GetProcAddress(dll, "afc_client_new");

	if(afc_client_new_func != NULL){
		afc_error_t result = afc_client_new_func(device, service, client);

		std::cout << "Success in getting afc_client_new. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_client_new." << std::endl;

		killProgram();
	}
}

void openFile(HMODULE dll, afc_client_t client, const char *filename, afc_file_mode_t file_mode, uint64_t *handle){
	openFileAFC openFileAFC_func = (openFileAFC)GetProcAddress(dll, "afc_file_open");

	if(openFileAFC_func != NULL){
		afc_error_t result = openFileAFC_func(client, filename, file_mode, handle);

		std::cout << "Success in getting afc_file_open. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_file_open." << std::endl;

		killProgram();
	}
}

void getFileInfo(HMODULE dll, afc_client_t client, const char *filename, char ***file_information){
	getFileInfoAFC getFileInfoAFC_func = (getFileInfoAFC)GetProcAddress(dll, "afc_get_file_info");

	if(getFileInfoAFC_func != NULL){
		afc_error_t result = getFileInfoAFC_func(client, filename, file_information);

		std::cout << "Success in getting afc_get_file_info. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_get_file_info." << std::endl;

		killProgram();
	}
}

void readFile(HMODULE dll, afc_client_t client, uint64_t handle, char *data, uint32_t length){
	readFileAFC readFileAFC_func = (readFileAFC)GetProcAddress(dll, "afc_file_read");

	uint32_t totalBytesRead = 0;

	if(readFileAFC_func != NULL){
		afc_error_t result = AFC_E_SUCCESS;

		uint32_t bytesRead = -1; //to enter the while loop
		uint32_t totalBytesLeftToRead = length;

		while(result == AFC_E_SUCCESS && bytesRead > 0){
			bytesRead = 0;

			//by passing in the totalBytesRead as an index, the machine will pick up at that spot once four megabytes has been transferred into the fileBuffer
			afc_error_t result = readFileAFC_func(client, handle, &data[totalBytesRead], totalBytesLeftToRead, &bytesRead);

			if(result == AFC_E_SUCCESS){
				totalBytesRead += bytesRead;
				totalBytesLeftToRead -= bytesRead;

				float currentAmountReceived = ((float)totalBytesRead / length) * 100;

				if(currentAmountReceived != 100){
					std::cout << currentAmountReceived << "% transferred..." << std::endl;
				}
			}
			else{
				std::cout << "afc_file_read failed. Result: " << result << std::endl;

				killProgram();
			}
		}

		std::cout << "100% transferred." << std::endl;

		std::cout << "Done reading file. Total bytes read = " << totalBytesRead << std::endl;
	}
	else{
		std::cout << "Error in getting afc_file_read." << std::endl;

		killProgram();
	}
}

void writeFile(HMODULE dll, afc_client_t client, uint64_t handle, const char *data, uint32_t length, uint32_t *bytes_written){
	writeFileAFC writeFileAFC_func = (writeFileAFC)GetProcAddress(dll, "afc_file_write");

	if(writeFileAFC_func != NULL){
		afc_error_t result = writeFileAFC_func(client, handle, data, length, bytes_written);

		std::cout << "Success in getting afc_write_file. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_file_write." << std::endl;

		killProgram();
	}
}

void closeFile(HMODULE dll, afc_client_t client, uint64_t handle){
	closeFileAFC closeFileAFC_func = (closeFileAFC)GetProcAddress(dll, "afc_file_close");

	if(closeFileAFC_func != NULL){
		afc_error_t result = closeFileAFC_func(client, handle);

		std::cout << "Success in getting afc_file_close. Result: " << result << std::endl;
	}
	else{
		std::cout << "Could not get afc_file_close." << std::endl;

		killProgram();
	}
}

void truncateFile(HMODULE dll, afc_client_t client, uint64_t handle, uint64_t newsize){
	truncateFileAFC truncateFileAFC_func = (truncateFileAFC)GetProcAddress(dll, "afc_file_truncate");

	if(truncateFileAFC_func != NULL){
		afc_error_t result = truncateFileAFC_func(client, handle, newsize);

		std::cout << "Success in getting afc_file_truncate. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_file_truncate." << std::endl;

		killProgram();
	}
}

void readDirectory(HMODULE dll, afc_client_t client, const char *path, char ***directory_information){
	readDirectoryAFC readDirectoryAFC_func = (readDirectoryAFC)GetProcAddress(dll, "afc_read_directory");

	if(readDirectoryAFC_func != NULL){
		afc_error_t result = readDirectoryAFC_func(client, path, directory_information);

		std::cout << "Success in getting afc_read_directory. Result: " << result << std::endl;
	}
	else{
		std::cout << "Error in getting afc_read_directory." << std::endl;

		killProgram();
	}
}

//not my code. I forget where I got this from.
std::wstring str_to_widestr(const std::string& s){
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

void killProgram(){
	_getch();

	exit(EXIT_FAILURE);
}