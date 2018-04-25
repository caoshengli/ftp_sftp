// ftp_sftp.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "ftp_sftp.h"
#include "cJSON.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "strsafe.h"
#include<vector>
// Ψһ��Ӧ�ó������

CWinApp theApp;

using namespace std;

static bool bSftp = false;
static wchar_t csIp[MAX_PATH] = {};
static wchar_t csPort[MAX_PATH] = {};
static INTERNET_PORT intPort;
static wchar_t csUsername[MAX_PATH] = {};
static wchar_t csPassword[MAX_PATH] = {};
static wchar_t csMode[MAX_PATH] = {};
static wchar_t wsDir[MAX_PATH] = {};

static HINTERNET hInetSession = NULL;
static HINTERNET hFtpConn = NULL;

static wchar_t wsPathSplit[] = L"\\|/"; //�ļ�·���ָ�������

/*

aim:�����Ժ���Ը��ݷָ����չѹ���ļ�����
author:wsh
time:20180403
input:
	string strTime:��Ҫ�ָ���ַ���
output:
	vector<WCHAR*>:�ָ�󱣴��vector����
*/
static std::vector<WCHAR*> split(const WCHAR* strTime)  
{  
	std::vector<WCHAR*> result;   
	int pos = 0;//�ַ���������ǣ����㽫���һ��������vector  
	size_t i;
	for( i = 0; i < wcslen(strTime); i++)  
	{  
		if(strTime[i] == '|')  
		{  
			WCHAR* temp = new WCHAR[i-pos+1];
			memset(temp,0x00,sizeof(WCHAR));
			wcsncpy(temp,strTime+pos,i-pos);
			temp[i-pos] = 0x00;
			result.push_back(temp); 
			pos=i+1;
		}  
	}  
	//�ж����һ��
	if (pos < i)
	{
		WCHAR* temp = new WCHAR[i-pos+1];
		memset(temp,0x00,sizeof(WCHAR));
		wcsncpy(temp,strTime+pos,i-pos);
		temp[i-pos] = 0x00;
		result.push_back(temp);
	}
	return result;  
} 
static const wchar_t* wcstrrchr(const wchar_t* str, const wchar_t wc)  
{  
	const wchar_t* pwc = NULL;  
	for (int i=wcslen(str)-1;i>=0;i--)  
	{  
		if (str[i] == wc)  
		{  
			pwc = str + i;  
			break;  
		}  
	}  
	return pwc;  
}  
static const wchar_t* wcstrrchr(const wchar_t* str, const wchar_t* wc) //wc��ʽ"\\|/"����'\\"��'/'2�ַָ�
{
	std::vector<WCHAR*>  vec = split(wc);
	
	const wchar_t* pwc = NULL; 
	for (int i=wcslen(str)-1;i>=0;i--)  
	{  
		for (std::vector<WCHAR*>::const_iterator itr = vec.cbegin();itr!=vec.cend();itr++)
		{	
			if (wcsncmp(&str[i],*itr,1) == 0)  
			{  
				pwc = str + i;  
				return pwc;  
			}  
		}
		
	}  
	return pwc;  
}
static void getPath_Name(const wchar_t* str, const wchar_t* wc,wchar_t* wsPath, wchar_t* wsName)
{
	const wchar_t* pwc = wcstrrchr(str,wc);
	memset(wsPath,0,sizeof(wsPath));  
	memset(wsName,0,sizeof(wsName));
	if (pwc)
	{
		for (int i=0; i<pwc-str; i++)  
			wsPath[i] = *(str+i); 
		wsprintf(wsName,L"%s",pwc+1);
	}
}

static bool createFtpMultiDir(const wchar_t* path)  
{  

	if (path == NULL || path == L"" || wcslen(path)<=0) return true;
	const wchar_t* pwcPath = wcstrrchr(path,L"/");
	wchar_t wsPath[MAX_PATH] = {};
	if (pwcPath)
	{
		for (int i=0; i<pwcPath-path; i++)  
			wsPath[i] = *(path+i); 
	}
	createFtpMultiDir(wsPath);
	if(FtpCreateDirectory(hFtpConn,path)) return true;  
	return false;  
}  

static bool createMultiDir(const wchar_t* path)  
{  
	if (path == NULL) return false;  
	if (PathIsDirectory(path)) return true;  

	wchar_t wsSubPath[MAX_PATH] = {}; 
	wchar_t wsSubName[MAX_PATH] = {};
	getPath_Name(path,wsPathSplit,wsSubPath,wsSubName); 
	if (wcslen(wsSubPath) )
		createMultiDir(wsSubPath);
	if(CreateDirectory(path,NULL)) return true;  
	return false;  
} 

const char* ftpopen(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root)
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	memset(csIp,0,sizeof(csIp));
	memset(csPort,0,sizeof(csPort));
	memset(csUsername,0,sizeof(csUsername));
	memset(csPassword,0,sizeof(csPassword));
	memset(csMode,0,sizeof(csMode));

	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"ip",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,csIp,sizeof(csIp)/sizeof(csIp[0]));
	GET_JASON_OBJECT(root,item,"port",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,csPort,sizeof(csPort)/sizeof(csPort[0]));
	intPort = (INTERNET_PORT)atoi(item->valuestring);
	GET_JASON_OBJECT(root,item,"username",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,csUsername,sizeof(csUsername)/sizeof(csUsername[0]));
	GET_JASON_OBJECT(root,item,"password",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,csPassword,sizeof(csPassword)/sizeof(csPassword[0]));
	GET_JASON_OBJECT(root,item,"mode",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,csMode,sizeof(csMode)/sizeof(csMode[0]));
	cJSON_Delete(root);
	bSftp = csMode[0] == L'1'?true:false;

	if (bSftp)
	{
		//�����ӿ�

	}
	else
	{
		hInetSession=InternetOpen(AfxGetAppName(),INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
		if(!hInetSession) return ret;
		hFtpConn=InternetConnect(hInetSession,csIp,intPort,
			csUsername,csPassword,INTERNET_SERVICE_FTP,INTERNET_FLAG_PASSIVE,0);
		if(!hFtpConn) 
		{
			InternetCloseHandle(hInetSession);
			RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"��������ʧ��","");
		}
	}
	
	RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
}

const char* cd(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root) 
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	memset(wsDir,0,sizeof(wsDir));
	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"dir",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsDir,sizeof(wsDir)/sizeof(wsDir[0]));
	cJSON_Delete(root);

	if (bSftp)
	{
		//sftp�ӿڱ���
	}
	else
	{
		if(wcslen(wsDir)!=0 && hFtpConn!=NULL && FtpSetCurrentDirectory(hFtpConn,wsDir))
		{
			RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
		}
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"���õ�ǰ·��ʧ��","");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}

//����Ҫ�������ļ���·������������Ӧ�ļ��� �õ��ļ�����Ҷ�ӽڵ� 
static bool TraverseDirectory(wchar_t* Dir,wchar_t* FtpDir,wchar_t* ExternDir)      
{  
	WIN32_FIND_DATA FindFileData;  
	HANDLE hFind=INVALID_HANDLE_VALUE;  
	wchar_t DirSpec[MAX_PATH];                  //����Ҫ�������ļ��е�Ŀ¼  
	wsprintf(DirSpec,L"%s\\*",Dir);
	
 
	bool bIsEmptyFolder = true;
	hFind=FindFirstFile(DirSpec,&FindFileData);          //�ҵ��ļ����еĵ�һ���ļ�  
	bool ret;
	if(hFind==INVALID_HANDLE_VALUE)                               //���hFind�������ʧ�ܣ����������Ϣ  
	{  
		ret =  false;    
	}  
	else   
	{   
		while(FindNextFile(hFind,&FindFileData)!=0)                            //���ļ������ļ��д���ʱ  
		{  
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0&&wcscmp(FindFileData.cFileName,L".")==0||wcscmp(FindFileData.cFileName,L"..")==0)        //�ж����ļ���&&��ʾΪ"."||��ʾΪ"."  
			{  
				continue;  
			}  
			wchar_t DirAdd[MAX_PATH];  
			StringCchCopy(DirAdd,MAX_PATH,Dir);  
			StringCchCat(DirAdd,MAX_PATH,TEXT("\\"));  
			StringCchCat(DirAdd,MAX_PATH,FindFileData.cFileName); 
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0)      //�ж�������ļ���  
			{  
				//ƴ�ӵõ����ļ��е�����·��  
				bIsEmptyFolder = false;
				wchar_t ExternDirAdd[MAX_PATH];
				StringCchCopy(ExternDirAdd,MAX_PATH,ExternDir);  
				StringCchCat(ExternDirAdd,MAX_PATH,TEXT("/")); 
				StringCchCat(ExternDirAdd,MAX_PATH,FindFileData.cFileName); 
				ret = TraverseDirectory(DirAdd,FtpDir,ExternDirAdd);                                  //ʵ�ֵݹ����  
				if(!ret)
				{
					FindClose(hFind);  
					return ret;
				}
			}  
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)    //��������ļ���  
			{  
				//wcout<<Dir<<"\\"<<FindFileData.cFileName<<endl;            //�������·��  
				bIsEmptyFolder = false;
				wchar_t FtpDirAdd[MAX_PATH] = {};
				StringCchCopy(FtpDirAdd,MAX_PATH,FtpDir);  
				StringCchCat(FtpDirAdd,MAX_PATH,ExternDir); 
				createFtpMultiDir(FtpDirAdd);
				FtpSetCurrentDirectory(hFtpConn,FtpDirAdd);
				SetCurrentDirectory(Dir);
				ret = FtpPutFile(hFtpConn,FindFileData.cFileName,FindFileData.cFileName,FTP_TRANSFER_TYPE_BINARY,INTERNET_FLAG_HYPERLINK);
			    if(!ret)
				{
					FindClose(hFind);  
					return ret;
				}
			}  
		}  
		if (bIsEmptyFolder)   //���ļ���
		{
			wchar_t FtpDirAdd[MAX_PATH] = {};
			StringCchCopy(FtpDirAdd,MAX_PATH,FtpDir); 
			StringCchCat(FtpDirAdd,MAX_PATH,ExternDir); 
			ret = createFtpMultiDir(FtpDirAdd);
		}
		FindClose(hFind);  
	} 
	return ret;
} 

const char* uploaddir(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root)
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	wchar_t wsDestDir[MAX_PATH] = {};
	wchar_t wsSrcDir[MAX_PATH] = {};
	memset(wsDestDir,0,sizeof(wsDestDir));
	memset(wsSrcDir,0,sizeof(wsSrcDir));

	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"srcdir",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsSrcDir,sizeof(wsSrcDir)/sizeof(wsSrcDir[0]));
	GET_JASON_OBJECT(root,item,"destdir",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsDestDir,sizeof(wsDestDir)/sizeof(wsDestDir[0]));
	cJSON_Delete(root);

	WIN32_FIND_DATA FindFileData;  
	HANDLE hFind=INVALID_HANDLE_VALUE;  
	hFind=FindFirstFile(wsSrcDir,&FindFileData);          //�ҵ��ļ����еĵ�һ���ļ�  
	if(hFind==INVALID_HANDLE_VALUE)                               //���hFind�������ʧ�ܣ����������Ϣ  
	{  
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"ԭ�ļ�������",""); 
	}  
	if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0) //�ļ�����δ���
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�����ļ��ж����ļ�,����uploadfile",""); 
	}

	if (bSftp)
	{
		//sftp����
	}
	else
	{
		if (hFtpConn == NULL) 
		{
			RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"����ʧ��","");
		}
		createFtpMultiDir(wsDestDir);
		//�ݹ��ϴ�
		if(TraverseDirectory(wsSrcDir,wsDestDir,L""))
		{
			RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
		}
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�ϴ��ļ���ʧ��","");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}
const char* uploadfile(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root)
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	wchar_t wsSrcfilename[MAX_PATH] = {};
	wchar_t wsDestfilename[MAX_PATH] = {};
	memset(wsSrcfilename,0,sizeof(wsSrcfilename));
	memset(wsDestfilename,0,sizeof(wsDestfilename));

	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"srcfilename",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsSrcfilename,sizeof(wsSrcfilename)/sizeof(wsSrcfilename[0]));
	GET_JASON_OBJECT(root,item,"destfilename",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsDestfilename,sizeof(wsDestfilename)/sizeof(wsDestfilename[0]));
	cJSON_Delete(root);

	WIN32_FIND_DATA FindFileData;  
	HANDLE hFind=INVALID_HANDLE_VALUE;  
	hFind=FindFirstFile(wsSrcfilename,&FindFileData);          //�ҵ��ļ����еĵ�һ���ļ�  
	if(hFind==INVALID_HANDLE_VALUE)                               //���hFind�������ʧ�ܣ����������Ϣ  
	{  
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"ԭ�ļ�������",""); 
	}  
	if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0) //�ļ��У���δ���
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�����ļ������ļ���,����uploaddir",""); 
	}

	if (bSftp)
	{
		//sftp����
	}
	else
	{
		if (hFtpConn == NULL) 
		{
			RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"����ʧ��","");
		}
	
		wchar_t wsSubPath[MAX_PATH] = {}; 
		wchar_t wsSubName[MAX_PATH] = {}; 
		getPath_Name(wsDestfilename,wsPathSplit,wsSubPath,wsSubName);
		if (wcslen(wsSubPath))
			createFtpMultiDir(wsSubPath);
		else
		{
			DWORD dwPathLen;
			FtpGetCurrentDirectory(hFtpConn,wsSubPath,&dwPathLen);
			wsprintf(wsSubName,L"%s",wsDestfilename);
		}
		FtpSetCurrentDirectory(hFtpConn,wsSubPath);

		if(FtpPutFile(hFtpConn,wsSrcfilename,wsSubName,FTP_TRANSFER_TYPE_BINARY,INTERNET_FLAG_HYPERLINK))
		{
			RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
		}
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�ϴ��ļ�ʧ��","");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}

static bool bNotFtpFindFirstFile = true;
static bool preDownloaddir(wchar_t* FtpDir,wchar_t* LocalDir,wchar_t* ExternDir) 
{
	bool bret = false;bool bIsEmptyFolder = true;
	WIN32_FIND_DATA findData;
	HINTERNET hFind = NULL;


	wchar_t wsLocalDirAdd[MAX_PATH] = {};
	StringCchCopy(wsLocalDirAdd,MAX_PATH,LocalDir);  
	StringCchCat(wsLocalDirAdd,MAX_PATH,ExternDir);
	createMultiDir(wsLocalDirAdd);

	HINTERNET hFtpConn_T=InternetConnect(hInetSession,csIp,intPort,csUsername,csPassword,INTERNET_SERVICE_FTP,INTERNET_FLAG_PASSIVE,0);
	FtpSetCurrentDirectory(hFtpConn_T,FtpDir);
	if(!(hFind=FtpFindFirstFile(hFtpConn_T,_T("*"),&findData,0,0)))
	{
		if (GetLastError()!= ERROR_NO_MORE_FILES)
			bret=FALSE;
		else
			bret=TRUE;
		InternetCloseHandle(hFtpConn_T);
		return bret;
	}
	
	do
	{
		if((findData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0&&wcscmp(findData.cFileName,L".")==0||wcscmp(findData.cFileName,L"..")==0)        //�ж����ļ���&&��ʾΪ"."||��ʾΪ"."  
		{  
			continue;  
		}  
		if(findData.dwFileAttributes==FILE_ATTRIBUTE_DIRECTORY)
		{
			bIsEmptyFolder = false;
			wchar_t wsFtpDirAdd[MAX_PATH] = {}; 
			wchar_t wsExternDirAdd[MAX_PATH] = {}; 
			StringCchCopy(wsFtpDirAdd,MAX_PATH,FtpDir);  
			StringCchCat(wsFtpDirAdd,MAX_PATH,TEXT("/")); 
			StringCchCat(wsFtpDirAdd,MAX_PATH,findData.cFileName);

			StringCchCopy(wsExternDirAdd,MAX_PATH,ExternDir);  
			StringCchCat(wsExternDirAdd,MAX_PATH,TEXT("\\")); 
			StringCchCat(wsExternDirAdd,MAX_PATH,findData.cFileName);
			bret = preDownloaddir(wsFtpDirAdd,LocalDir,wsExternDirAdd);
			if (!bret)
			{
				InternetCloseHandle(hFtpConn_T);
				return bret;
			}
		}
		else
		{
			SetCurrentDirectory(wsLocalDirAdd);
			bret = FtpGetFile(hFtpConn,findData.cFileName,findData.cFileName,FALSE,FILE_ATTRIBUTE_NORMAL,FTP_TRANSFER_TYPE_BINARY|INTERNET_FLAG_NO_CACHE_WRITE,0);
			if (!bret)
			{
				InternetCloseHandle(hFtpConn_T);
				break;
			}
		}
	}while(InternetFindNextFile(hFind,&findData));
	InternetCloseHandle(hFtpConn_T);
	return bret;
}
const char* downloaddir(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root)
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	wchar_t wsDestDir[MAX_PATH] = {};
	wchar_t wsSrcDir[MAX_PATH] = {};
	memset(wsDestDir,0,sizeof(wsDestDir));
	memset(wsSrcDir,0,sizeof(wsSrcDir));

	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"srcdir",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsSrcDir,sizeof(wsSrcDir)/sizeof(wsSrcDir[0]));
	GET_JASON_OBJECT(root,item,"destdir",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsDestDir,sizeof(wsDestDir)/sizeof(wsDestDir[0]));
	cJSON_Delete(root);

	if (bSftp)
	{
		//sftp����
	}
	else
	{
		if (hFtpConn == NULL) 
		{
			RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"����ʧ��","");
		}
		createMultiDir(wsDestDir);
		FtpSetCurrentDirectory(hFtpConn,wsSrcDir);

		if(preDownloaddir(wsSrcDir,wsDestDir,L""))
		{
			RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
		}
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�����ļ���ʧ��","");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}

const char* downloadfile(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	if (!root)
	{
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"json����ʧ��","");
	}

	wchar_t wsSrcfilename[MAX_PATH] = {};
	wchar_t wsDestfilename[MAX_PATH] = {};
	memset(wsSrcfilename,0,sizeof(wsSrcfilename));
	memset(wsDestfilename,0,sizeof(wsDestfilename));

	cJSON * item = NULL;
	GET_JASON_OBJECT(root,item,"srcfilename",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsSrcfilename,sizeof(wsSrcfilename)/sizeof(wsSrcfilename[0]));
	GET_JASON_OBJECT(root,item,"destfilename",ret);
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,wsDestfilename,sizeof(wsDestfilename)/sizeof(wsDestfilename[0]));
	cJSON_Delete(root);

	if (bSftp)
	{
		//sftp����
	}
	else
	{
		if (hFtpConn == NULL) 
		{
			RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"����ʧ��","");
		}
		const wchar_t* pwcSrcfilename = wcstrrchr(wsSrcfilename,L'/');
		wchar_t wsSubPath[MAX_PATH] = {}; 
		wchar_t wsSubName[MAX_PATH] = {}; 
		getPath_Name(wsSrcfilename,wsPathSplit,wsSubPath,wsSubName);
		if (wcslen(wsSubPath) <= 0)
		{
			DWORD dwPathLen;
			FtpGetCurrentDirectory(hFtpConn,wsSubPath,&dwPathLen);
			wsprintf(wsSubName,L"%s",wsSrcfilename);
		}
		FtpSetCurrentDirectory(hFtpConn,wsSubPath);
		wchar_t wsDestPath[MAX_PATH] = {}; 
		wchar_t wsDestName[MAX_PATH] = {}; 
		getPath_Name(wsDestfilename,wsPathSplit,wsDestPath,wsDestName);
		if (wcslen(wsDestPath) <= 0)
		{
			GetCurrentDirectory(MAX_PATH,wsDestPath);
			wsprintf(wsDestName,L"%s",wsDestfilename);
		}
		createMultiDir(wsDestPath);
		SetCurrentDirectory(wsDestPath);

		if(FtpGetFile(hFtpConn,wsSubName,wsDestName,FALSE,FILE_ATTRIBUTE_NORMAL,FTP_TRANSFER_TYPE_BINARY|
			INTERNET_FLAG_NO_CACHE_WRITE,0))
		{
			RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
		}
		RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"�����ļ�ʧ��","");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}
const char* ftpclose(const char* jason)
{
	char* ret = new char[LEN_1024];
	if (bSftp)
	{
		//sftp����
	}
	else
	{
		if (hFtpConn != NULL)
		{
			InternetCloseHandle(hFtpConn);
		}
		if (hInetSession != NULL)
		{
			InternetCloseHandle(hInetSession);
		}
		RETURN_SUCCESS(ret,GWI_PLUGIN_SUCCESS,"");
	}
	RETURN_ERROR(ret,GWI_PLUGIN_ERROR,"","");
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// ��ʼ�� MFC ����ʧ��ʱ��ʾ����
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			// TODO: ���Ĵ�������Է���������Ҫ
			_tprintf(_T("����: MFC ��ʼ��ʧ��\n"));
			nRetCode = 1;
		}
		else
		{
			
			// TODO: �ڴ˴�ΪӦ�ó������Ϊ��д���롣
			const char csOpen[] = "{\"ip\":\"172.20.10.3\",\"port\":\"21\",\"username\":\"dell\",\"password\":\"wsh\",\"mode\":\"0\"}";
		    const char csCd[] = "{\"dir\":\"/test\"}";
			const char csDownloadFile[] = "{\"srcfilename\":\"/test/test1.txt\",\"destfilename\":\"d:\\\\ftp_test\\\\test1.txt\"}";
		    const char csDownloadDir[] = "{\"srcdir\":\"/test\",\"destdir\":\"D:\\\\ftp_test\"}";
			const char csUpdateFile[] = "{\"srcfilename\":\"d:\\\\ftp_test\\\\gwi.txt\",\"destfilename\":\"/gwi/txt/g.txt\"}";
			const char csUpdateDir[] = "{\"srcdir\":\"d:\\\\ftp_test\",\"destdir\":\"/gwi/txt\"}";
			const char* pcRet = NULL;
			pcRet = ftpopen(csOpen);
			printf("%s\n",pcRet);
			//getchar();
			pcRet = cd(csCd);
			printf("%s\n",pcRet);
			//getchar();
			/*
			pcRet = downloadfile(csDownloadFile);
			printf("%s\n",pcRet);
			//getchar();
			*/
			/*
			pcRet = downloaddir(csDownloadDir);
			printf("%s\n",pcRet);
			getchar();
			*/

			/*
			pcRet = uploadfile(csUpdateFile);
			printf("%s\n",pcRet);
			getchar();
			*/

			pcRet = uploaddir(csUpdateDir);
			printf("%s\n",pcRet);
			getchar();

			pcRet = ftpclose(NULL);
			printf("%s\n",pcRet);
			getchar();
			
	
		}
	}
	else
	{
		// TODO: ���Ĵ�������Է���������Ҫ
		_tprintf(_T("����: GetModuleHandle ʧ��\n"));
		nRetCode = 1;
	}

	return nRetCode;
}
