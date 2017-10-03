#include "stdafx.h"
#include "service_control.h"
#include "regstry_control.h"

service_control::service_control(_In_ const wchar_t* binary_path,
								 _In_ const wchar_t* service_name,
								 _In_ const wchar_t* service_display_name,
								 _In_ const wchar_t* altitude,
								 _In_ uint32_t flags,
								 _In_ bool unistall_service)
	: _unistall_service(unistall_service),
	_driver_handle(INVALID_HANDLE_VALUE),
	_binary_path(binary_path),
	_service_name(service_name),
	_service_display_name(service_display_name),
	_service_type(SERVICE_KERNEL_DRIVER),
	_is_minifilter(true),
	_altitude(altitude),
	_flags(flags)
{
}
service_control::~service_control()
{
	if (INVALID_HANDLE_VALUE != _driver_handle)
	{
		CloseHandle(_driver_handle); 
		_driver_handle = INVALID_HANDLE_VALUE;
	}

	if (true == _unistall_service)
	{
		stop_service();
		uninstall_service();
	}
}

bool service_control::install_service(bool auto_start)
{
	SC_HANDLE scm_handle = OpenSCManagerW(NULL,
										  NULL,
										  SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		printf("OpenSCManagerW() failed. gle=%u\n", GetLastError());
		return false;
	}

	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle),
								 sc_handle_deleter());


	if (true != uninstall_service(scm_handle))
	{
		printf("uninstall_service() failed.\n");
		return false;
	}

	//
	//	���� ��ġ
	//
	DWORD start_type = SERVICE_DEMAND_START;
	if (true == auto_start)
	{
		printf("���� �̱���~\n");
	}

	SC_HANDLE service_handle = CreateServiceW(scm_handle,
											  _service_name.c_str(),
											  _service_display_name.c_str(),
											  GENERIC_READ, // SERVICE_ALL_ACCESS,
											  _service_type,
											  start_type,
											  SERVICE_ERROR_NORMAL,
											  _binary_path.c_str(),
											  NULL,
											  NULL,
											  NULL,
											  NULL,
											  NULL);
	if (NULL == service_handle)
	{
		printf("CreateServiceW(%ws) failed. gle=%u\n",
			   _service_name.c_str(),
			   GetLastError());
		return false;
	}
	CloseServiceHandle(service_handle);

	//
	//	�̴� ���� ����̹���� �߰� �����ؾ��� 1
	//
	//	key  : HKLM\SYSTEM\CurrentControlSet\Services\[xxx]\Instances
	//	value: "DefaultInstance" = "AltitudeAndFlags"
	//

	bool ret = false;
	HKEY key_handle = NULL;
	std::wstringstream key_path;
	do
	{
#define	MF_DEFAULT_INSTANCE	L"DefaultInstance"
#define	MF_ALTITUDE_N_FLAGS	L"AltitudeAndFlags"			
		key_path
			<< L"SYSTEM\\CurrentControlSet\\Services\\"
			<< _service_name
			<< L"\\Instances";

		//
		//	Instances Ű�� �����Ƿ� �����Ѵ� (���� �ִٸ� open �Ѵ�).
		// 
		key_handle = RUCreateKey(HKEY_LOCAL_MACHINE,
								 key_path.str().c_str(),
								 false);
		if (NULL == key_handle)
		{
			printf("RUCreateKey() failed. key=%ws\n",
				   key_path.str().c_str());
			break;
		}

		if (!RUSetString(key_handle,
						 MF_DEFAULT_INSTANCE,
						 MF_ALTITUDE_N_FLAGS))
		{
			printf("RUSetString(HKLM, %ws, %ws) failed.\n",
				   key_path.str().c_str(),
				   MF_DEFAULT_INSTANCE);
			break;
		}

		//
		//	ok
		//
		ret = true;

	} while (false);


	RUCloseKey(key_handle);
	key_handle = NULL;

	if (!ret)
	{
		return false;
	}

	//
	//	�̴� ���� ����̹���� �߰� �����ؾ��� 2
	// 
	//	key  : HKLM\SYSTEM\CurrentControlSet\Services\scanner\Instances\AltitudeAndFlags
	//	value:	"Altitude" = "0"
	//			"Flags" = dword:00000000
	//
	ret = false;
	do
	{
		_ASSERTE(key_handle == NULL);

#define	MF_ALTITUDE		L"Altitude"
#define	MF_FLAGS		L"Flags"
		std::wstringstream key_path;
		key_path.str(L"");
		key_path.clear();

		key_path
			<< L"SYSTEM\\CurrentControlSet\\Services\\"
			<< _service_name
			<< L"\\Instances\\AltitudeAndFlags";

		key_handle = RUCreateKey(HKEY_LOCAL_MACHINE,
								 key_path.str().c_str(),
								 false);
		if (nullptr == key_handle)
		{
			printf("RUCreateKey() failed. key=%ws\n",
				   key_path.str().c_str());
			break;
		}

		if (!RUSetString(key_handle,
						 MF_ALTITUDE,
						 _altitude.c_str()))
		{
			printf("RUSetString(HKLM, %ws, %ws) failed.\n",
				   key_path.str().c_str(),
				   MF_ALTITUDE);
			break;
		}

		if (!RUWriteDword(key_handle,
						  MF_FLAGS,
						  _flags))
		{
			printf("RUWriteDword(HKLM, %ws, %ws) failed.\n",
				   key_path.str().c_str(),
				   MF_FLAGS);
			break;
		}

		//
		//	OK
		// 
		ret = true;

	} while (false);

	RUCloseKey(key_handle);
	key_handle = NULL;

	return ret;
}

bool service_control::start_service()
{
	SC_HANDLE scm_handle = OpenSCManagerW(NULL,
										  NULL,
										  SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		printf("OpenSCManagerW() faield. gle = %u\n",
			   GetLastError());
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle),
								 sc_handle_deleter());

	SC_HANDLE service_handle = OpenServiceW(scm_handle,
											_service_name.c_str(),
											SERVICE_ALL_ACCESS);
	if (NULL == service_handle)
	{
		printf("OpenServiceW( service_name=%ws ) failed. gle = %u\n",
			   _service_name.c_str(),
			   GetLastError());
		return false;
	}
	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle),
									 sc_handle_deleter());

	//
	//	�̹� �������̶�� true �� �����Ѵ�. 
	//
	{
		bool started = false;
		if (true != service_started(service_handle, started))
		{
			printf("service_started() failed. service=%ws\n",
				   _service_name.c_str());
			return false;
		}
		if (true == started) return true;
	}

	//
	//	���� ����
	//
	if (true != start_service(service_handle))
	{
		printf("start_service() failed.\n");
		return false;
	}

	return true;
}

bool service_control::start_service(SC_HANDLE service_handle)
{

	_ASSERTE(NULL != service_handle);
	if (NULL == service_handle) return false;

	if (TRUE != StartService(service_handle,
							 0,
							 NULL))
	{
		DWORD err = GetLastError();
		if (err != ERROR_SERVICE_ALREADY_RUNNING)
		{
			printf("StartService( service name=%ws ) failed, gle = %u\n",
				   _service_name.c_str(),
				   err);
			return false;
		}
	}

	//
	//	���񽺰� ���۵� ������ ��ٸ���. 
	// 
	SERVICE_STATUS svc_status = { 0 };
	while (QueryServiceStatus(service_handle, &svc_status))
	{
		if (svc_status.dwCurrentState == SERVICE_START_PENDING)
		{
			Sleep(1000);
		}
		else
		{
			break;
		}
	}

	if (svc_status.dwCurrentState != SERVICE_RUNNING)
	{
		printf("service start failed. svc=%ws\n",
			   _service_name.c_str());

		return false;
	}

	printf("service is started. svc=%ws\n",
		   _service_name.c_str());

	return true;
}

bool service_control::service_started(
	_In_ SC_HANDLE service_handle,
	_Out_ bool& started
)
{
	_ASSERTE(NULL != service_handle);
	if (NULL == service_handle)
	{
		return false;
	}

	//
	//	���񽺰� ���¸� ��ȸ�Ѵ�. 
	// 
	SERVICE_STATUS svc_status = { 0 };
	if (!QueryServiceStatus(service_handle, &svc_status))
	{
		printf("QueryServiceStatus(%ws) failed.\n",
			   _service_name.c_str());
		return false;
	}

	if (svc_status.dwCurrentState == SERVICE_RUNNING)
	{
		started = true;
	}
	else
	{
		started = false;
	}

	return true;

	return false;
}

bool service_control::stop_service()
{
	SC_HANDLE scm_handle = OpenSCManagerW(NULL,
										  NULL,
										  SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		printf("OpenSCManagerW() faield. gle = %u\n",
			   GetLastError()
		);
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle),
								 sc_handle_deleter());

	//
	//	���񽺰� ��ġ�Ǿ����� �ʴٸ� true �� �����Ѵ�.
	//
	{
		bool installed = false;
		if (!service_installed(scm_handle, installed))
		{
			printf("service_installed() failed. service=%ws\n",
				   _service_name.c_str()
			);
			return false;
		}

		if (true != installed)
		{
			return true;
		}
	}

	//
	//	���� ����
	//
	SC_HANDLE service_handle = OpenServiceW(scm_handle,
											_service_name.c_str(),
											SERVICE_ALL_ACCESS);
	if (NULL == service_handle)
	{
		printf("OpenServiceW( ) failed. service=%ws, gle = %u\n",
			   _service_name.c_str(),
			   GetLastError());
		return false;
	}
	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle),
									 sc_handle_deleter());

	//
	//	���񽺰� ���������� �ʴٸ� true �� �����Ѵ�. 
	//
	{
		bool started = false;
		if (!service_started(service_handle, started))
		{
			printf("service_started() failed. service=%ws\n",
				   _service_name.c_str());
			return false;
		}

		if (true != started) return true;
	}

	//
	//	���񽺸� �����Ѵ�. 
	// 
	if (true != stop_service(service_handle))
	{
		printf("stop_service() failed.\n");
		return false;
	}

	return true;
}

bool service_control::stop_service(SC_HANDLE service_handle)
{
	_ASSERTE(NULL != service_handle);
	if (NULL == service_handle)
	{
		return false;
	}

	SERVICE_STATUS service_status = { 0 };
	if (FALSE == ControlService(service_handle,
								SERVICE_CONTROL_STOP,
								&service_status))
	{
		printf("ControlService() failed. gle=%u\n",
			   GetLastError());
		return false;
	}

	//
	//	���񽺰� ����� ������ ��ٸ���. 
	// 
	SERVICE_STATUS svc_status = { 0 };
	while (QueryServiceStatus(service_handle, &svc_status))
	{
		if (svc_status.dwCurrentState == SERVICE_STOP_PENDING)
		{
			Sleep(1000);
		}
		else
		{
			break;
		}
	}
	if (svc_status.dwCurrentState == SERVICE_STOPPED)
	{
		printf("service has stopped. svc=%ws\n",
			   _service_name.c_str());
	}
	else
	{
		printf("service stop failed. svc=%ws\n",
			   _service_name.c_str());
		//
		//	���� ���� ���´� �ƴ����� ����Ǿ��� ���� ���´� �ƴϹǷ�
		//	����ó���ʰ�, �����Ѵ�. 
		// 
	}

	return true;
}

bool service_control::service_installed(
	_In_ SC_HANDLE scm_handle,
	_Out_ bool& installed
)
{
	_ASSERTE(NULL != scm_handle);
	if (NULL == scm_handle)
	{
		return false;
	}

	SC_HANDLE service_handle = OpenServiceW(scm_handle,
											_service_name.c_str(),
											SERVICE_QUERY_STATUS);

	if (NULL == service_handle)
	{
		DWORD gle = GetLastError();
		if (ERROR_SERVICE_DOES_NOT_EXIST == gle)
		{
			installed = false;
			return true;
		}
		else
		{
			printf("OpenServiceW(%ws) failed. gle=%u\n",
				   _service_name.c_str(),
				   GetLastError());
			return false;
		}
	}

	CloseServiceHandle(service_handle);
	installed = true;

	return true;
}

bool service_control::uninstall_service()
{
	SC_HANDLE scm_handle = OpenSCManagerW(NULL,
										  NULL,
										  SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		printf("OpenSCManagerW() failed. gle=%u\n", GetLastError());
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle),
								 sc_handle_deleter());

	return uninstall_service(scm_handle);
}

bool service_control::uninstall_service(_In_ SC_HANDLE scm_handle)
{
	_ASSERTE(NULL != scm_handle);
	if (NULL == scm_handle)
	{
		return false;
	}

	//
	// ���񽺰� ��ġ �Ǿ� ���� ������ true ��ȯ
	//
	{
		bool installed = false;
		if (true != service_installed(scm_handle, installed))
		{
			printf("service_installed(%ws) failed.\n",
				   _service_name.c_str());
			return false;
		}

		if (true != installed)
		{
			return true;
		}
	}

	SC_HANDLE service_handle = OpenServiceW(scm_handle,
											_service_name.c_str(),
											SERVICE_ALL_ACCESS);
	if (NULL == service_handle)
	{
		printf("OpenServiceW(%ws) failed. gle=%u\n",
			   _service_name.c_str(),
			   GetLastError());
		return false;
	}

	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle),
									 sc_handle_deleter());

	//
	//	���� ���̶�� ����
	//
	{
		bool started = false;
		if (true != service_started(service_handle, started))
		{
			printf("service_started() failed.\n");
			return false;
		}

		if (true == started)
		{
			if (true != stop_service(service_handle))
			{
				printf("stop_service() failed.\n");
			}
		}
	}

	//
	//	���� ����
	//
	if (FALSE == DeleteService(service_handle))
	{
		DWORD err = GetLastError();
		if (ERROR_SERVICE_MARKED_FOR_DELETE == err)
		{
			printf("Service marked for delete (need reboot). service=%ws\n",
				   _service_name.c_str());
			return true;
		}
		else
		{
			printf("DeleteService() failed, gle = %u\n",
				   GetLastError());
			return false;
		}
	}

	printf("Service deleted successfully. service=%ws\n",
		   _service_name.c_str());

	return true;
}
