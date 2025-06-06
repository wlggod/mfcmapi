// Common functions for MFCMAPI

#include <StdAfx.h>
#include <UI/Dialogs/MFCUtilityFunctions.h>
#include <core/mapi/mapiStoreFunctions.h>
#include <core/mapi/columnTags.h>
#include <UI/Dialogs/BaseDialog.h>
#include <core/mapi/cache/mapiObjects.h>
#include <UI/Dialogs/Editors/Editor.h>
#include <core/smartview/SmartView.h>
#include <core/interpret/guid.h>
#include <UI/Dialogs/HierarchyTable/MsgStoreDlg.h>
#include <UI/Dialogs/ContentsTable/FolderDlg.h>
#include <UI/Dialogs/ContentsTable/RecipientsDlg.h>
#include <UI/Dialogs/ContentsTable/AttachmentsDlg.h>
#include <UI/Dialogs/propList/SingleMessageDialog.h>
#include <UI/Dialogs/propList/SingleRecipientDialog.h>
#include <UI/Dialogs/ContentsTable/ABDlg.h>
#include <UI/Dialogs/ContentsTable/RulesDlg.h>
#include <UI/Dialogs/ContentsTable/AclDlg.h>
#include <UI/Dialogs/ContentsTable/MailboxTableDlg.h>
#include <UI/Dialogs/ContentsTable/PublicFolderTableDlg.h>
#include <core/utility/registry.h>
#include <core/mapi/mapiOutput.h>
#include <core/utility/strings.h>
#include <core/utility/output.h>
#include <core/mapi/mapiFunctions.h>

namespace dialog
{
	_Check_return_ HRESULT DisplayObject(
		_In_ LPMAPIPROP lpUnk,
		ULONG ulObjType,
		objectType tType,
		_In_opt_ CBaseDialog* lpHostDlg,
		_In_ std::shared_ptr<cache::CMapiObjects> lpMapiObjects)
	{
		if (!lpMapiObjects || !lpUnk) return MAPI_E_INVALID_PARAMETER;

		// If we weren't passed an object type, go get one - careful! Some objects lie!
		if (!ulObjType)
		{
			ulObjType = mapi::GetMAPIObjectType(lpUnk);
		}

		auto szFlags = smartview::InterpretNumberAsStringProp(ulObjType, PR_OBJECT_TYPE);
		output::DebugPrint(
			output::dbgLevel::Generic,
			L"DisplayObject asked to display %p, with objectType of 0x%08X and MAPI type of 0x%08X = %ws\n",
			lpUnk,
			tType,
			ulObjType,
			szFlags.c_str());

		// call the dialog
		switch (ulObjType)
		{
			// #define MAPI_STORE ((ULONG) 0x00000001) /* Message Store */
		case MAPI_STORE:
		{
			if (lpHostDlg)
			{
				lpHostDlg->OnUpdateSingleMAPIPropListCtrl(lpUnk, nullptr);
			}

			auto lpTempMDB = mapi::safe_cast<LPMDB>(lpUnk);

			auto lpMDB = lpMapiObjects->GetMDB(); // do not release
			if (lpMDB) lpMDB->AddRef(); // hold on to this so that...
			lpMapiObjects->SetMDB(lpTempMDB);

			new CMsgStoreDlg(
				lpMapiObjects,
				lpUnk,
				nullptr,
				objectType::storeDeletedItems == tType ? tableDisplayFlags::dfDeleted : tableDisplayFlags::dfNormal);

			// restore the old MDB
			lpMapiObjects->SetMDB(lpMDB); // ...we can put it back
			if (lpMDB) lpMDB->Release();
			if (lpTempMDB) lpTempMDB->Release();
			break;
		}
		// #define MAPI_FOLDER ((ULONG) 0x00000003) /* Folder */
		case MAPI_FOLDER:
		{
			// There are two ways to display a folder...either the contents table or the hierarchy table.
			if (tType == objectType::hierarchy || tType == objectType::otDefault)
			{
				const auto lpMDB = lpMapiObjects->GetMDB(); // do not release
				if (lpMDB)
				{
					new CMsgStoreDlg(lpMapiObjects, lpMDB, lpUnk, tableDisplayFlags::dfNormal);
				}
				else
				{
					// Since lpMDB was NULL, let's get a good MDB
					const auto lpMAPISession = lpMapiObjects->GetSession(); // do not release
					if (lpMAPISession)
					{
						auto lpNewMDB = mapi::store::OpenStoreFromMAPIProp(lpMAPISession, lpUnk);
						if (lpNewMDB)
						{
							lpMapiObjects->SetMDB(lpNewMDB);
							new CMsgStoreDlg(lpMapiObjects, lpNewMDB, lpUnk, tableDisplayFlags::dfNormal);

							// restore the old MDB
							lpMapiObjects->SetMDB(nullptr);
							lpNewMDB->Release();
						}
					}
				}
			}
			else if (tType == objectType::contents || tType == objectType::assocContents)
			{
				new CFolderDlg(
					lpMapiObjects,
					lpUnk,
					tType == objectType::assocContents ? tableDisplayFlags::dfAssoc : tableDisplayFlags::dfNormal);
			}
		}
		break;
		// #define MAPI_ABCONT ((ULONG) 0x00000004) /* Address Book Container */
		case MAPI_ABCONT:
			new CAbDlg(lpMapiObjects, lpUnk);
			break;
			// #define MAPI_MESSAGE ((ULONG) 0x00000005) /* Message */
		case MAPI_MESSAGE:
			new SingleMessageDialog(lpMapiObjects, lpUnk);
			break;
			// #define MAPI_MAILUSER ((ULONG) 0x00000006) /* Individual Recipient */
		case MAPI_MAILUSER:
			new SingleRecipientDialog(lpMapiObjects, lpUnk);
			break;
			// #define MAPI_DISTLIST ((ULONG) 0x00000008) /* Distribution List Recipient */
		case MAPI_DISTLIST: // A DistList is really an Address book anyways
			new SingleRecipientDialog(lpMapiObjects, lpUnk);
			new CAbDlg(lpMapiObjects, lpUnk);
			break;
			// The following types don't have special viewers - just dump their props in the property pane
			// #define MAPI_ADDRBOOK ((ULONG) 0x00000002) /* Address Book */
			// #define MAPI_ATTACH ((ULONG) 0x00000007) /* Attachment */
			// #define MAPI_PROFSECT ((ULONG) 0x00000009) /* Profile Section */
			// #define MAPI_STATUS ((ULONG) 0x0000000A) /* Status Object */
			// #define MAPI_SESSION ((ULONG) 0x0000000B) /* Session */
			// #define MAPI_FORMINFO ((ULONG) 0x0000000C) /* Form Information */
		default:
			if (lpHostDlg)
			{
				lpHostDlg->OnUpdateSingleMAPIPropListCtrl(lpUnk, nullptr);
			}

			szFlags = smartview::InterpretNumberAsStringProp(ulObjType, PR_OBJECT_TYPE);
			output::DebugPrint(
				output::dbgLevel::Generic,
				L"DisplayObject: Object type: 0x%08X = %ws not implemented\r\n" // STRING_OK
				L"This is not an error. It just means no specialized viewer has been implemented for this object "
				L"type.", // STRING_OK
				ulObjType,
				szFlags.c_str());
			break;
		}

		return S_OK;
	}

	_Check_return_ HRESULT
	DisplayObject(_In_ LPMAPIPROP lpUnk, ULONG ulObjType, objectType tType, _In_ CBaseDialog* lpHostDlg)
	{
		if (!lpHostDlg || !lpUnk) return MAPI_E_INVALID_PARAMETER;

		auto lpMapiObjects = lpHostDlg->GetMapiObjects(); // do not release
		if (!lpMapiObjects) return MAPI_E_INVALID_PARAMETER;

		return DisplayObject(lpUnk, ulObjType, tType, lpHostDlg, lpMapiObjects);
	}

	_Check_return_ HRESULT DisplayTable(_In_ LPMAPITABLE lpTable, objectType tType, _In_ CBaseDialog* lpHostDlg)
	{
		if (!lpHostDlg) return MAPI_E_INVALID_PARAMETER;

		const auto lpMapiObjects = lpHostDlg->GetMapiObjects(); // do not release
		if (!lpMapiObjects) return MAPI_E_INVALID_PARAMETER;

		output::DebugPrint(output::dbgLevel::Generic, L"DisplayTable asked to display %p\n", lpTable);

		switch (tType)
		{
		case objectType::status:
		{
			if (!lpTable) return MAPI_E_INVALID_PARAMETER;
			new CContentsTableDlg(
				lpMapiObjects,
				IDS_STATUSTABLE,
				createDialogType::CALL_CREATE_DIALOG,
				nullptr,
				lpTable,
				&columns::sptSTATUSCols.tags,
				columns::STATUSColumns,
				NULL,
				MENU_CONTEXT_STATUS_TABLE);
			break;
		}
		case objectType::receive:
		{
			if (!lpTable) return MAPI_E_INVALID_PARAMETER;
			new CContentsTableDlg(
				lpMapiObjects,
				IDS_RECEIVEFOLDERTABLE,
				createDialogType::CALL_CREATE_DIALOG,
				nullptr,
				lpTable,
				&columns::sptRECEIVECols.tags,
				columns::RECEIVEColumns,
				NULL,
				MENU_CONTEXT_RECIEVE_FOLDER_TABLE);
			break;
		}
		case objectType::hierarchy:
		{
			if (!lpTable) return MAPI_E_INVALID_PARAMETER;
			new CContentsTableDlg(
				lpMapiObjects,
				IDS_HIERARCHYTABLE,
				createDialogType::CALL_CREATE_DIALOG,
				nullptr,
				lpTable,
				&columns::sptHIERARCHYCols.tags,
				columns::HIERARCHYColumns,
				NULL,
				MENU_CONTEXT_HIER_TABLE);
			break;
		}
		default:
		case objectType::otDefault:
		{
			if (!lpTable) return MAPI_E_INVALID_PARAMETER;
			if (tType != objectType::otDefault) error::ErrDialog(__FILE__, __LINE__, IDS_EDDISPLAYTABLE, tType);
			new CContentsTableDlg(
				lpMapiObjects,
				IDS_CONTENTSTABLE,
				createDialogType::CALL_CREATE_DIALOG,
				nullptr,
				lpTable,
				&columns::sptDEFCols.tags,
				columns::DEFColumns,
				NULL,
				MENU_CONTEXT_DEFAULT_TABLE);
			break;
		}
		}

		return S_OK;
	}

	_Check_return_ HRESULT
	DisplayTable(_In_ LPMAPIPROP lpMAPIProp, ULONG ulPropTag, objectType tType, _In_ CBaseDialog* lpHostDlg)
	{
		LPMAPITABLE lpTable = nullptr;

		if (!lpHostDlg || !lpMAPIProp) return MAPI_E_INVALID_PARAMETER;
		if (PT_OBJECT != PROP_TYPE(ulPropTag)) return MAPI_E_INVALID_TYPE;

		auto unicodeFlag = registry::preferUnicodeProps ? MAPI_UNICODE : fMapiUnicode;
		auto hRes = WC_MAPI(lpMAPIProp->OpenProperty(
			ulPropTag, &IID_IMAPITable, unicodeFlag, 0, reinterpret_cast<LPUNKNOWN*>(&lpTable)));
		if (hRes == MAPI_E_INTERFACE_NOT_SUPPORTED)
		{
			hRes = S_OK;
			switch (PROP_ID(ulPropTag))
			{
			case PROP_ID(PR_MESSAGE_ATTACHMENTS):
			{
				auto lpMessage = mapi::safe_cast<LPMESSAGE>(lpMAPIProp);
				if (lpMessage)
				{
					lpMessage->GetAttachmentTable(unicodeFlag, &lpTable);
					lpMessage->Release();
				}
				break;
			}
			case PROP_ID(PR_MESSAGE_RECIPIENTS):
			{
				auto lpMessage = mapi::safe_cast<LPMESSAGE>(lpMAPIProp);
				if (lpMessage)
				{
					lpMessage->GetRecipientTable(unicodeFlag, &lpTable);
					lpMessage->Release();
				}
				break;
			}
			}
		}

		if (lpTable)
		{
			switch (PROP_ID(ulPropTag))
			{
			case PROP_ID(PR_MESSAGE_ATTACHMENTS):
				new CAttachmentsDlg(lpHostDlg->GetMapiObjects(), lpTable, lpMAPIProp);
				break;
			case PROP_ID(PR_MESSAGE_RECIPIENTS):
				new CRecipientsDlg(lpHostDlg->GetMapiObjects(), lpTable, lpMAPIProp);
				break;
			default:
				hRes = EC_H(DisplayTable(lpTable, tType, lpHostDlg));
				break;
			}

			lpTable->Release();
		}

		return hRes;
	}

	_Check_return_ HRESULT
	DisplayExchangeTable(_In_ LPMAPIPROP lpMAPIProp, ULONG ulPropTag, objectType tType, _In_ CBaseDialog* lpHostDlg)
	{
		LPEXCHANGEMODIFYTABLE lpExchTbl = nullptr;
		LPMAPITABLE lpMAPITable = nullptr;

		if (!lpMAPIProp || !lpHostDlg) return MAPI_E_INVALID_PARAMETER;

		const auto lpMapiObjects = lpHostDlg->GetMapiObjects(); // do not release
		if (!lpMapiObjects) return MAPI_E_INVALID_PARAMETER;

		// Open the table in an IExchangeModifyTable interface
		auto hRes = EC_MAPI(lpMAPIProp->OpenProperty(
			ulPropTag,
			const_cast<LPGUID>(&IID_IExchangeModifyTable),
			0,
			MAPI_DEFERRED_ERRORS,
			reinterpret_cast<LPUNKNOWN*>(&lpExchTbl)));

		if (lpExchTbl)
		{
			switch (tType)
			{
			case objectType::rules:
				new CRulesDlg(lpMapiObjects, lpExchTbl);
				break;
			case objectType::ACL:
			{
				editor::CEditor MyData(
					lpHostDlg, IDS_ACLTABLE, IDS_ACLTABLEPROMPT, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL);

				MyData.AddPane(viewpane::CheckPane::Create(0, IDS_FBRIGHTSVISIBLE, false, false));

				if (MyData.DisplayDialog())
				{
					new CAclDlg(lpMapiObjects, lpExchTbl, MyData.GetCheck(0));
				}
			}
			break;
			default:
				// Open a MAPI table on the Exchange table property. This table can be
				// read to determine what the Exchange table looks like.
				hRes = EC_MAPI(lpExchTbl->GetTable(0, &lpMAPITable));

				if (lpMAPITable)
				{
					hRes = EC_H(DisplayTable(lpMAPITable, tType, lpHostDlg));
					lpMAPITable->Release();
				}

				break;
			}

			lpExchTbl->Release();
		}

		return hRes;
	}

	_Check_return_ bool bShouldCancel(_In_opt_ CWnd* cWnd, HRESULT hResPrev)
	{
		auto bGotError = false;

		if (S_OK != hResPrev)
		{
			if (MAPI_E_USER_CANCEL != hResPrev && MAPI_E_CANCEL != hResPrev)
			{
				bGotError = true;
			}

			editor::CEditor Cancel(cWnd, ID_PRODUCTNAME, IDS_CANCELPROMPT, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL);
			if (bGotError)
			{
				const auto szPrevErr =
					strings::formatmessage(IDS_PREVIOUSCALL, error::ErrorNameFromErrorCode(hResPrev).c_str(), hResPrev);
				Cancel.AddPane(viewpane::TextPane::CreateSingleLinePane(0, IDS_ERROR, szPrevErr, true));
			}

			if (Cancel.DisplayDialog())
			{
				output::DebugPrint(output::dbgLevel::Generic, L"bShouldCancel: User asked to cancel\n");
				return true;
			}
		}

		return false;
	}

	void DisplayMailboxTable(_In_ std::shared_ptr<cache::CMapiObjects> lpMapiObjects)
	{
		if (!lpMapiObjects) return;
		LPMDB lpPrivateMDB = nullptr;
		auto lpMDB = lpMapiObjects->GetMDB(); // do not release
		const auto lpMAPISession = lpMapiObjects->GetSession(); // do not release

		// try the 'current' MDB first
		if (!mapi::store::StoreSupportsManageStore(lpMDB))
		{
			// if that MDB doesn't support manage store, try to get one that does
			lpPrivateMDB = mapi::store::OpenMessageStoreGUID(lpMAPISession, pbExchangeProviderPrimaryUserGuid);
			lpMDB = lpPrivateMDB;
		}

		if (lpMDB && mapi::store::StoreSupportsManageStore(lpMDB))
		{
			LPMAPITABLE lpMailboxTable = nullptr;
			const auto szServerName = mapi::store::GetServerName(lpMAPISession);

			editor::CEditor MyData(
				nullptr, IDS_DISPLAYMAILBOXTABLE, IDS_SERVERNAMEPROMPT, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(0, IDS_SERVERNAME, szServerName, false));
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(1, IDS_OFFSET, false));
			MyData.SetHex(1, 0);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(2, IDS_MAILBOXGUID, false));
			UINT uidDropDown[] = {IDS_GETMBXINTERFACE1, IDS_GETMBXINTERFACE3, IDS_GETMBXINTERFACE5};
			MyData.AddPane(
				viewpane::DropDownPane::Create(3, IDS_GETMBXINTERFACE, _countof(uidDropDown), uidDropDown, true));

			if (MyData.DisplayDialog())
			{
				if (0 != MyData.GetHex(1) && 0 == MyData.GetDropDown(3))
				{
					error::ErrDialog(__FILE__, __LINE__, IDS_EDOFFSETWITHWRONGINTERFACE);
				}
				else
				{
					auto szServerDN = mapi::store::BuildServerDN(MyData.GetStringW(0), L"");
					if (!szServerDN.empty())
					{
						LPMDB lpOldMDB = nullptr;

						// if we got a new MDB, set it in lpMapiObjects
						if (lpPrivateMDB)
						{
							lpOldMDB = lpMapiObjects->GetMDB(); // do not release
							if (lpOldMDB) lpOldMDB->AddRef(); // hold on to this so that...
							// If we don't do this, we crash when destroying the Mailbox Table Window
							lpMapiObjects->SetMDB(lpMDB);
						}

						switch (MyData.GetDropDown(3))
						{
						case 0:
							lpMailboxTable = mapi::store::GetMailboxTable1(lpMDB, szServerDN, fMapiUnicode);
							break;
						case 1:
							lpMailboxTable =
								mapi::store::GetMailboxTable3(lpMDB, szServerDN, MyData.GetHex(1), fMapiUnicode);
							break;
						case 2:
						{
							GUID MyGUID = {0};
							auto bHaveGUID = false;

							auto pszGUID = MyData.GetStringW(2);

							if (!pszGUID.empty())
							{
								bHaveGUID = true;

								MyGUID = guid::StringToGUID(pszGUID);
								if (MyGUID == GUID_NULL)
								{
									error::ErrDialog(__FILE__, __LINE__, IDS_EDINVALIDGUID);
									break;
								}
							}

							lpMailboxTable = mapi::store::GetMailboxTable5(
								lpMDB, szServerDN, MyData.GetHex(1), fMapiUnicode, bHaveGUID ? &MyGUID : nullptr);
							break;
						}
						}

						if (lpMailboxTable)
						{
							new CMailboxTableDlg(lpMapiObjects, MyData.GetStringW(0), lpMailboxTable);
							lpMailboxTable->Release();
						}
						else
						{
							error::ErrDialog(
								__FILE__,
								__LINE__,
								IDS_EDGETMAILBOXTABLEFAILED,
								_T("GetMailboxTable"),
								_T("GetMailboxTable")); // STRING_OK
						}

						if (lpOldMDB)
						{
							lpMapiObjects->SetMDB(lpOldMDB); // ...we can put it back
							if (lpOldMDB) lpOldMDB->Release();
						}
					}
				}
			}
		}

		if (lpPrivateMDB) lpPrivateMDB->Release();
	}

	void DisplayPublicFolderTable(_In_ std::shared_ptr<cache::CMapiObjects> lpMapiObjects)
	{
		if (!lpMapiObjects) return;
		LPMDB lpPrivateMDB = nullptr;
		auto lpMDB = lpMapiObjects->GetMDB(); // do not release
		const auto lpMAPISession = lpMapiObjects->GetSession(); // do not release

		// try the 'current' MDB first
		if (!mapi::store::StoreSupportsManageStore(lpMDB))
		{
			// if that MDB doesn't support manage store, try to get one that does
			lpPrivateMDB = mapi::store::OpenMessageStoreGUID(lpMAPISession, pbExchangeProviderPrimaryUserGuid);
			lpMDB = lpPrivateMDB;
		}

		if (lpMDB && mapi::store::StoreSupportsManageStore(lpMDB))
		{
			LPMAPITABLE lpPFTable = nullptr;
			const auto szServerName = mapi::store::GetServerName(lpMAPISession);

			editor::CEditor MyData(
				nullptr, IDS_DISPLAYPFTABLE, IDS_DISPLAYPFTABLEPROMPT, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(0, IDS_SERVERNAME, szServerName, false));
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(1, IDS_OFFSET, false));
			MyData.SetHex(1, 0);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(2, IDS_FLAGS, false));
			MyData.SetHex(2, MDB_IPM);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(3, IDS_PUBLICFOLDERGUID, false));
			UINT uidDropDown[] = {IDS_GETPFINTERFACE1, IDS_GETPFINTERFACE4, IDS_GETPFINTERFACE5};
			MyData.AddPane(
				viewpane::DropDownPane::Create(4, IDS_GETMBXINTERFACE, _countof(uidDropDown), uidDropDown, true));
			if (MyData.DisplayDialog())
			{
				if (0 != MyData.GetHex(1) && 0 == MyData.GetDropDown(4))
				{
					error::ErrDialog(__FILE__, __LINE__, IDS_EDOFFSETWITHWRONGINTERFACE);
				}
				else
				{
					auto szServerDN = mapi::store::BuildServerDN(MyData.GetStringW(0), L"");
					if (!szServerDN.empty())
					{
						LPMDB lpOldMDB = nullptr;

						// if we got a new MDB, set it in lpMapiObjects
						if (lpPrivateMDB)
						{
							lpOldMDB = lpMapiObjects->GetMDB(); // do not release
							if (lpOldMDB) lpOldMDB->AddRef(); // hold on to this so that...
							// If we don't do this, we crash when destroying the Mailbox Table Window
							lpMapiObjects->SetMDB(lpMDB);
						}

						switch (MyData.GetDropDown(4))
						{
						case 0:
							lpPFTable =
								mapi::store::GetPublicFolderTable1(lpMDB, szServerDN, MyData.GetHex(2) | fMapiUnicode);
							break;
						case 1:
							lpPFTable = mapi::store::GetPublicFolderTable4(
								lpMDB, szServerDN, MyData.GetHex(1), MyData.GetHex(2) | fMapiUnicode);
							break;
						case 2:
						{
							GUID MyGUID = {0};
							auto bHaveGUID = false;

							auto pszGUID = MyData.GetStringW(3);
							if (!pszGUID.empty())
							{
								bHaveGUID = true;

								MyGUID = guid::StringToGUID(pszGUID);
								if (MyGUID == GUID_NULL)
								{
									error::ErrDialog(__FILE__, __LINE__, IDS_EDINVALIDGUID);
									break;
								}
							}

							lpPFTable = mapi::store::GetPublicFolderTable5(
								lpMDB,
								szServerDN,
								MyData.GetHex(1),
								MyData.GetHex(2) | fMapiUnicode,
								bHaveGUID ? &MyGUID : nullptr);
							break;
						}
						}

						if (lpPFTable)
						{
							new CPublicFolderTableDlg(lpMapiObjects, MyData.GetStringW(0), lpPFTable);
							lpPFTable->Release();
						}
						else
						{
							error::ErrDialog(
								__FILE__,
								__LINE__,
								IDS_EDGETMAILBOXTABLEFAILED,
								_T("GetPublicFolderTable"),
								_T("GetPublicFolderTable")); // STRING_OK
						}

						if (lpOldMDB)
						{
							lpMapiObjects->SetMDB(lpOldMDB); // ...we can put it back
							if (lpOldMDB) lpOldMDB->Release();
						}
					}
				}
			}
		}

		if (lpPrivateMDB) lpPrivateMDB->Release();
	}

	_Check_return_ LPMAPIFORMINFO
	ResolveMessageClass(_In_ std::shared_ptr<cache::CMapiObjects> lpMapiObjects, _In_opt_ LPMAPIFOLDER lpMAPIFolder)
	{
		LPMAPIFORMMGR lpMAPIFormMgr = nullptr;
		if (!lpMapiObjects) return nullptr;

		const auto lpMAPISession = lpMapiObjects->GetSession(); // do not release
		if (!lpMAPISession) nullptr;

		LPMAPIFORMINFO lpMAPIFormInfo = nullptr;
		EC_MAPI_S(MAPIOpenFormMgr(lpMAPISession, &lpMAPIFormMgr));
		if (lpMAPIFormMgr)
		{
			output::DebugPrint(output::dbgLevel::Forms, L"OnResolveMessageClass: resolving message class\n");
			editor::CEditor MyData(
				nullptr, IDS_RESOLVECLASS, IDS_RESOLVECLASSPROMPT, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL);
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(0, IDS_CLASS, false));
			MyData.AddPane(viewpane::TextPane::CreateSingleLinePane(1, IDS_FLAGS, false));

			if (MyData.DisplayDialog())
			{
				auto szClass = MyData.GetStringW(0);
				const auto ulFlags = MyData.GetHex(1);
				if (!szClass.empty())
				{
					output::DebugPrint(
						output::dbgLevel::Forms,
						L"OnResolveMessageClass: Calling ResolveMessageClass(\"%ws\",0x%08X)\n",
						szClass.c_str(),
						ulFlags); // STRING_OK
					// ResolveMessageClass requires an ANSI string
					EC_MAPI_S(lpMAPIFormMgr->ResolveMessageClass(
						strings::wstringTostring(szClass).c_str(), ulFlags, lpMAPIFolder, &lpMAPIFormInfo));
					if (lpMAPIFormInfo)
					{
						output::outputFormInfo(output::dbgLevel::Forms, nullptr, lpMAPIFormInfo);
					}
				}
			}

			lpMAPIFormMgr->Release();
		}

		return lpMAPIFormInfo;
	}

	_Check_return_ LPMAPIFORMINFO SelectForm(
		_In_ HWND hWnd,
		_In_ std::shared_ptr<cache::CMapiObjects> lpMapiObjects,
		_In_opt_ LPMAPIFOLDER lpMAPIFolder)
	{
		LPMAPIFORMMGR lpMAPIFormMgr = nullptr;
		LPMAPIFORMINFO lpMAPIFormInfo = nullptr;

		if (!lpMapiObjects) return nullptr;

		const auto lpMAPISession = lpMapiObjects->GetSession(); // do not release
		if (!lpMAPISession) return nullptr;

		EC_MAPI_S(MAPIOpenFormMgr(lpMAPISession, &lpMAPIFormMgr));

		if (lpMAPIFormMgr)
		{
			// Apparently, SelectForm doesn't support unicode
			auto szTitle = strings::wstringTostring(strings::loadstring(IDS_SELECTFORMPROPS));
			EC_H_CANCEL_S(lpMAPIFormMgr->SelectForm(
				reinterpret_cast<ULONG_PTR>(hWnd),
				0, // fMapiUnicode,
				strings::LPCSTRToLPTSTR(szTitle.c_str()),
				lpMAPIFolder,
				&lpMAPIFormInfo));

			if (lpMAPIFormInfo)
			{
				output::outputFormInfo(output::dbgLevel::Forms, nullptr, lpMAPIFormInfo);
			}

			lpMAPIFormMgr->Release();
		}

		return lpMAPIFormInfo;
	}
} // namespace dialog