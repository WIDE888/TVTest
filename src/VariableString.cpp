#include "stdafx.h"
#include <algorithm>
#include "TVTest.h"
#include "VariableString.h"
#include "AppMain.h"
#include "Common/DebugDef.h"


namespace TVTest
{


bool FormatVariableString(CVariableStringMap *pVariableMap,LPCWSTR pszFormat,String *pString)
{
	if (pString==nullptr)
		return false;

	pString->clear();

	if (pszFormat==nullptr || pVariableMap==nullptr)
		return false;

	if (!pVariableMap->BeginFormat())
		return false;

	struct SeparatorInfo {
		String::size_type Pos;
		LPCWSTR pszSeparator;
		SeparatorInfo(String::size_type p,LPCWSTR s) : Pos(p), pszSeparator(s) {}
	};

	std::vector<SeparatorInfo> SeparatorList;
	LPCWSTR p=pszFormat;

	while (*p!=L'\0') {
		if (*p==L'%') {
			p++;
			if (*p==L'%') {
				pString->append(L"%");
				p++;
			} else {
				String Keyword,Text;

				while (*p!=L'%' && *p!=L'\0')
					Keyword.push_back(*p++);
				if (*p==L'%') {
					p++;
					if (::lstrcmpiW(Keyword.c_str(),L"sep-hyphen")==0) {
						SeparatorList.push_back(SeparatorInfo(pString->size(),L"-"));
					} else if (::lstrcmpiW(Keyword.c_str(),L"sep-slash")==0) {
						SeparatorList.push_back(SeparatorInfo(pString->size(),L"/"));
					} else if (::lstrcmpiW(Keyword.c_str(),L"sep-backslash")==0) {
						SeparatorList.push_back(SeparatorInfo(pString->size(),L"\\"));
					} else if (pVariableMap->GetString(Keyword.c_str(),&Text)) {
						pVariableMap->NormalizeString(&Text);
						pString->append(Text);
					} else {
						pString->append(L"%");
						pVariableMap->NormalizeString(&Keyword);
						pString->append(Keyword);
						pString->append(L"%");
					}
				} else {
					pString->append(L"%");
					pVariableMap->NormalizeString(&Keyword);
					pString->append(Keyword);
				}
			}
		} else {
			pString->push_back(*p++);
		}
	}

	if (!SeparatorList.empty()) {
		// 前後にトークンが存在する場合のみ区切りを挿入する
		bool fLast=true;
		for (int i=static_cast<int>(SeparatorList.size()-1);i>=0;i--) {
			auto itBegin=pString->begin()+(i>0 ? SeparatorList[i-1].Pos : 0);
			auto itEnd=pString->begin()+
				(i+1<static_cast<int>(SeparatorList.size()) ? SeparatorList[i+1].Pos : pString->length());
			auto itCur=pString->begin()+SeparatorList[i].Pos;
			bool fPrev=
				std::find_if(itBegin,itCur,[](WCHAR c) -> bool { return c!=L' '; })!=itCur;
			bool fNext=
				std::find_if(itCur,itEnd,[](WCHAR c) -> bool { return c!=L' '; })!=itEnd;
			if ((fPrev && fNext) || (fPrev && !fLast))
				pString->insert(SeparatorList[i].Pos,SeparatorList[i].pszSeparator);
			if (fPrev || fNext)
				fLast=false;
		}
	}

	pVariableMap->EndFormat();

	return true;
}




bool CBasicVariableStringMap::GetString(LPCWSTR pszKeyword,String *pString)
{
	if (GetPreferredGlobalString(pszKeyword,pString))
		return true;
	bool fResult=false;
	if (GetLocalString(pszKeyword,pString)) {
		fResult=true;
		if (!pString->empty())
			return true;
	}
	if (GetGlobalString(pszKeyword,pString))
		fResult=true;
	return fResult;
}


bool CBasicVariableStringMap::GetPreferredGlobalString(LPCWSTR pszKeyword,String *pString)
{
	return GetAppClass().VariableManager.GetPreferredVariable(pszKeyword,pString);
}


bool CBasicVariableStringMap::GetGlobalString(LPCWSTR pszKeyword,String *pString)
{
	return GetAppClass().VariableManager.GetVariable(pszKeyword,pString);
}


bool CBasicVariableStringMap::GetParameterList(ParameterGroupList *pList) const
{
	std::vector<CVariableManager::VariableInfo> VarList;

	if (!GetAppClass().VariableManager.GetVariableList(&VarList))
		return false;
	if (VarList.empty())
		return true;

	pList->push_back(ParameterGroup());
	ParameterGroup &Group=pList->back();

	Group.ParameterList.reserve(VarList.size());

	for (auto it=VarList.begin();it!=VarList.end();++it) {
		bool fFound=false;

		for (size_t i=0;i<pList->size()-1;i++) {
			for (size_t j=0;j<(*pList)[i].ParameterList.size();j++) {
				if (::lstrcmpiW((*pList)[i].ParameterList[j].pszParameter,it->pszKeyword)==0) {
					fFound=true;
					break;
				}
			}
		}

		if (!fFound) {
			ParameterInfo Info;

			Info.pszParameter=it->pszKeyword;
			Info.pszText=it->pszDescription;
			Group.ParameterList.push_back(Info);
		}
	}

	return true;
}




bool CVariableStringMap::InputParameter(HWND hDlg,int EditID,const POINT &MenuPos)
{
	ParameterGroupList GroupList;

	if (!GetParameterList(&GroupList) || GroupList.empty())
		return false;

	HMENU hmenuRoot=::CreatePopupMenu();

	for (size_t i=0;i<GroupList.size();i++) {
		const ParameterGroup &Group=GroupList[i];
		HMENU hmenu;

		if (!Group.Text.empty()) {
			hmenu=::CreatePopupMenu();
			::AppendMenu(hmenuRoot,MF_POPUP | MF_STRING | MF_ENABLED,
						 reinterpret_cast<UINT_PTR>(hmenu),
						 Group.Text.c_str());
		} else {
			hmenu=hmenuRoot;
		}

		for (int j=0;j<(int)Group.ParameterList.size();j++) {
			const ParameterInfo &Param=Group.ParameterList[j];

			TCHAR szText[128];
			StdUtil::snprintf(szText,lengthof(szText),TEXT("%s\t%%%s%%"),
							  Param.pszText,Param.pszParameter);
			::AppendMenu(hmenu,MF_STRING | MF_ENABLED,(i<<10)|(j+1),szText);
		}
	}

	int Command=::TrackPopupMenu(hmenuRoot,TPM_RETURNCMD,MenuPos.x,MenuPos.y,0,hDlg,NULL);
	::DestroyMenu(hmenuRoot);
	if (Command<=0)
		return false;

	const int GroupIndex=Command>>10;
	const int ParamIndex=(Command&0x3FF)-1;
	if (GroupIndex>=(int)GroupList.size()
			|| ParamIndex<0
			|| ParamIndex>=(int)GroupList[GroupIndex].ParameterList.size())
		return false;
	String Param;
	Param=L"%";
	Param+=GroupList[GroupIndex].ParameterList[ParamIndex].pszParameter;
	Param+=L"%";
	DWORD Start,End;
	::SendDlgItemMessage(hDlg,EditID,EM_GETSEL,
		reinterpret_cast<WPARAM>(&Start),reinterpret_cast<LPARAM>(&End));
	::SendDlgItemMessage(hDlg,EditID,EM_REPLACESEL,
		TRUE,reinterpret_cast<LPARAM>(Param.c_str()));
	::SetFocus(::GetDlgItem(hDlg,EditID));
	if (End<Start)
		Start=End;
	::SendDlgItemMessage(hDlg,EditID,EM_SETSEL,
		Start,Start+Param.length());

	return true;
}


bool CVariableStringMap::GetTimeString(LPCWSTR pszKeyword,const SYSTEMTIME &Time,String *pString) const
{
	if (::lstrcmpiW(pszKeyword,L"date")==0) {
		StringUtility::Format(*pString,L"%d%02d%02d",
							  Time.wYear,Time.wMonth,Time.wDay);
	} else if (::lstrcmpiW(pszKeyword,L"time")==0) {
		StringUtility::Format(*pString,L"%02d%02d%02d",
							  Time.wHour,Time.wMinute,Time.wSecond);
	} else if (::lstrcmpiW(pszKeyword,L"year")==0) {
		StringUtility::Format(*pString,L"%d",Time.wYear);
	} else if (::lstrcmpiW(pszKeyword,L"year2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wYear%100);
	} else if (::lstrcmpiW(pszKeyword,L"month")==0) {
		StringUtility::Format(*pString,L"%d",Time.wMonth);
	} else if (::lstrcmpiW(pszKeyword,L"month2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wMonth);
	} else if (::lstrcmpiW(pszKeyword,L"day")==0) {
		StringUtility::Format(*pString,L"%d",Time.wDay);
	} else if (::lstrcmpiW(pszKeyword,L"day2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wDay);
	} else if (::lstrcmpiW(pszKeyword,L"hour")==0) {
		StringUtility::Format(*pString,L"%d",Time.wHour);
	} else if (::lstrcmpiW(pszKeyword,L"hour2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wHour);
	} else if (::lstrcmpiW(pszKeyword,L"minute")==0) {
		StringUtility::Format(*pString,L"%d",Time.wMinute);
	} else if (::lstrcmpiW(pszKeyword,L"minute2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wMinute);
	} else if (::lstrcmpiW(pszKeyword,L"second")==0) {
		StringUtility::Format(*pString,L"%d",Time.wSecond);
	} else if (::lstrcmpiW(pszKeyword,L"second2")==0) {
		StringUtility::Format(*pString,L"%02d",Time.wSecond);
	} else if (::lstrcmpiW(pszKeyword,L"day-of-week")==0) {
		*pString=GetDayOfWeekText(Time.wDayOfWeek);
	} else {
		return false;
	}

	return true;
}


bool CVariableStringMap::IsDateTimeParameter(LPCTSTR pszKeyword)
{
	static const LPCTSTR ParameterList[] = {
		TEXT("date"),
		TEXT("year"),
		TEXT("year2"),
		TEXT("month"),
		TEXT("month2"),
		TEXT("day"),
		TEXT("day2"),
		TEXT("time"),
		TEXT("hour"),
		TEXT("hour2"),
		TEXT("minute"),
		TEXT("minute2"),
		TEXT("second"),
		TEXT("second2"),
		TEXT("day-of-week"),
	};

	for (int i=0;i<lengthof(ParameterList);i++) {
		if (::lstrcmpi(ParameterList[i],pszKeyword)==0)
			return true;
	}

	return false;
}




CEventVariableStringMap::CEventVariableStringMap()
	: m_Flags(0)
	, m_fCurrentTimeSet(false)
{
}


CEventVariableStringMap::CEventVariableStringMap(const EventInfo &Info)
	: m_Flags(0)
	, m_EventInfo(Info)
	, m_fCurrentTimeSet(false)
{
}


bool CEventVariableStringMap::BeginFormat()
{
	if (!m_fCurrentTimeSet)
		::GetLocalTime(&m_CurrentTime);

	return true;
}


bool CEventVariableStringMap::GetLocalString(LPCWSTR pszKeyword,String *pString)
{
	if (::lstrcmpi(pszKeyword,TEXT("channel-name"))==0) {
		*pString=m_EventInfo.Channel.GetName();
	} else if (::lstrcmpi(pszKeyword,TEXT("channel-no"))==0) {
		if (m_EventInfo.Channel.GetChannelNo()==0)
			return false;
		StringUtility::Format(*pString,TEXT("%d"),m_EventInfo.Channel.GetChannelNo());
	} else if (::lstrcmpi(pszKeyword,TEXT("channel-no2"))==0) {
		if (m_EventInfo.Channel.GetChannelNo()==0)
			return false;
		StringUtility::Format(*pString,TEXT("%02d"),m_EventInfo.Channel.GetChannelNo());
	} else if (::lstrcmpi(pszKeyword,TEXT("channel-no3"))==0) {
		if (m_EventInfo.Channel.GetChannelNo()==0)
			return false;
		StringUtility::Format(*pString,TEXT("%03d"),m_EventInfo.Channel.GetChannelNo());
	} else if (::lstrcmpi(pszKeyword,TEXT("event-name"))==0) {
		*pString=m_EventInfo.Event.m_EventName;
	} else if (::lstrcmpi(pszKeyword,TEXT("event-title"))==0) {
		GetEventTitle(m_EventInfo.Event.m_EventName,pString);
	} else if (::lstrcmpi(pszKeyword,TEXT("event-mark"))==0) {
		GetEventMark(m_EventInfo.Event.m_EventName,pString);
	} else if (::lstrcmpi(pszKeyword,TEXT("event-id"))==0) {
		StringUtility::Format(*pString,TEXT("%04X"),m_EventInfo.Event.m_EventID);
	} else if (::lstrcmpi(pszKeyword,TEXT("service-name"))==0) {
		*pString=m_EventInfo.ServiceName;
	} else if (::lstrcmpi(pszKeyword,TEXT("service-id"))==0) {
		StringUtility::Format(*pString,TEXT("%04X"),m_EventInfo.Event.m_ServiceID);
	} else if (::lstrcmpi(pszKeyword,TEXT("tuner-filename"))==0) {
		*pString=m_EventInfo.Channel.GetTunerName();
	} else if (::lstrcmpi(pszKeyword,TEXT("tuner-name"))==0) {
		LPCTSTR pszTuner=::PathFindFileName(m_EventInfo.Channel.GetTunerName());
		if (::StrCmpNI(pszTuner,TEXT("BonDriver_"),10)==0)
			pszTuner+=10;
		pString->assign(pszTuner,::PathFindExtension(pszTuner)-pszTuner);
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-hour"))==0) {
		StringUtility::Format(*pString,TEXT("%d"),
							  (int)(m_EventInfo.Event.m_Duration/(60*60)));
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-hour2"))==0) {
		StringUtility::Format(*pString,TEXT("%02d"),
							  (int)(m_EventInfo.Event.m_Duration/(60*60)));
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-min"))==0) {
		StringUtility::Format(*pString,TEXT("%d"),
							  (int)(m_EventInfo.Event.m_Duration/60%60));
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-min2"))==0) {
		StringUtility::Format(*pString,TEXT("%02d"),
							  (int)(m_EventInfo.Event.m_Duration/60%60));
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-sec"))==0) {
		StringUtility::Format(*pString,TEXT("%d"),
							  (int)(m_EventInfo.Event.m_Duration%60));
	} else if (::lstrcmpi(pszKeyword,TEXT("event-duration-sec2"))==0) {
		StringUtility::Format(*pString,TEXT("%02d"),
							  (int)(m_EventInfo.Event.m_Duration%60));
	} else if (IsDateTimeParameter(pszKeyword)) {
		if ((m_Flags & FLAG_NO_CURRENT_TIME)!=0)
			return false;
		return GetTimeString(pszKeyword,m_CurrentTime,pString);
	} else if (::StrCmpNI(pszKeyword,TEXT("start-"),6)==0
			&& IsDateTimeParameter(pszKeyword+6)) {
		if (!m_EventInfo.Event.m_bValidStartTime) {
			*pString=TEXT("x");
			return true;
		}
		return GetTimeString(pszKeyword+6,m_EventInfo.Event.m_StartTime,pString);
	} else if (::StrCmpNI(pszKeyword,TEXT("end-"),4)==0
			&& IsDateTimeParameter(pszKeyword+4)) {
		SYSTEMTIME EndTime;
		if (!m_EventInfo.Event.GetEndTime(&EndTime)) {
			*pString=TEXT("x");
			return true;
		}
		return GetTimeString(pszKeyword+4,EndTime,pString);
	} else if (::StrCmpNI(pszKeyword,TEXT("tot-"),4)==0
			&& IsDateTimeParameter(pszKeyword+4)) {
		if ((m_Flags & FLAG_NO_TOT_TIME)!=0)
			return false;
		return GetTimeString(pszKeyword+4,m_EventInfo.TotTime,pString);
	} else {
		return false;
	}

	return true;
}


bool CEventVariableStringMap::NormalizeString(String *pString) const
{
	if ((m_Flags & FLAG_NO_NORMALIZE)!=0)
		return false;

	// ファイル名に使用できない文字を置き換える
	for (auto i=pString->begin();i!=pString->end();++i) {
		static const struct {
			WCHAR From;
			WCHAR To;
		} CharMap[] = {
			{L'\\',	L'￥'},
			{L'/',	L'／'},
			{L':',	L'：'},
			{L'*',	L'＊'},
			{L'?',	L'？'},
			{L'"',	L'”'},
			{L'<',	L'＜'},
			{L'>',	L'＞'},
			{L'|',	L'｜'},
		};

		for (int j=0;j<lengthof(CharMap);j++) {
			if (CharMap[j].From==*i) {
				*i=CharMap[j].To;
				break;
			}
		}
	}

	return true;
}


bool CEventVariableStringMap::GetParameterList(ParameterGroupList *pList) const
{
	if (pList==nullptr)
		return false;

	static const ParameterInfo DateTimeParams[] =
	{
		{TEXT("date"),					TEXT("年月日")},
		{TEXT("year"),					TEXT("年")},
		{TEXT("year2"),					TEXT("年(下2桁)")},
		{TEXT("month"),					TEXT("月")},
		{TEXT("month2"),				TEXT("月(2桁)")},
		{TEXT("day"),					TEXT("日")},
		{TEXT("day2"),					TEXT("日(2桁)")},
		{TEXT("time"),					TEXT("時刻(時+分+秒)")},
		{TEXT("hour"),					TEXT("時")},
		{TEXT("hour2"),					TEXT("時(2桁)")},
		{TEXT("minute"),				TEXT("分")},
		{TEXT("minute2"),				TEXT("分(2桁)")},
		{TEXT("second"),				TEXT("秒")},
		{TEXT("second2"),				TEXT("秒(2桁)")},
		{TEXT("day-of-week"),			TEXT("曜日(漢字)")},
	};

	static const ParameterInfo StartTimeParams[] =
	{
		{TEXT("start-date"),			TEXT("年月日")},
		{TEXT("start-year"),			TEXT("年")},
		{TEXT("start-year2"),			TEXT("年(下2桁)")},
		{TEXT("start-month"),			TEXT("月")},
		{TEXT("start-month2"),			TEXT("月(2桁)")},
		{TEXT("start-day"),				TEXT("日")},
		{TEXT("start-day2"),			TEXT("日(2桁)")},
		{TEXT("start-time"),			TEXT("時刻(時+分+秒)")},
		{TEXT("start-hour"),			TEXT("時")},
		{TEXT("start-hour2"),			TEXT("時(2桁)")},
		{TEXT("start-minute"),			TEXT("分")},
		{TEXT("start-minute2"),			TEXT("分(2桁)")},
		{TEXT("start-second"),			TEXT("秒")},
		{TEXT("start-second2"),			TEXT("秒(2桁)")},
		{TEXT("start-day-of-week"),		TEXT("曜日(漢字)")},
	};

	static const ParameterInfo EndTimeParams[] =
	{
		{TEXT("end-date"),				TEXT("年月日")},
		{TEXT("end-year"),				TEXT("年")},
		{TEXT("end-year2"),				TEXT("年(下2桁)")},
		{TEXT("end-month"),				TEXT("月")},
		{TEXT("end-month2"),			TEXT("月(2桁)")},
		{TEXT("end-day"),				TEXT("日")},
		{TEXT("end-day2"),				TEXT("日(2桁)")},
		{TEXT("end-time"),				TEXT("時刻(時+分+秒)")},
		{TEXT("end-hour"),				TEXT("時")},
		{TEXT("end-hour2"),				TEXT("時(2桁)")},
		{TEXT("end-minute"),			TEXT("分")},
		{TEXT("end-minute2"),			TEXT("分(2桁)")},
		{TEXT("end-second"),			TEXT("秒")},
		{TEXT("end-second2"),			TEXT("秒(2桁)")},
		{TEXT("end-day-of-week"),		TEXT("曜日(漢字)")},
	};

	static const ParameterInfo TotTimeParams[] =
	{
		{TEXT("tot-date"),				TEXT("年月日")},
		{TEXT("tot-year"),				TEXT("年")},
		{TEXT("tot-year2"),				TEXT("年(下2桁)")},
		{TEXT("tot-month"),				TEXT("月")},
		{TEXT("tot-month2"),			TEXT("月(2桁)")},
		{TEXT("tot-day"),				TEXT("日")},
		{TEXT("tot-day2"),				TEXT("日(2桁)")},
		{TEXT("tot-time"),				TEXT("時刻(時+分+秒)")},
		{TEXT("tot-hour"),				TEXT("時")},
		{TEXT("tot-hour2"),				TEXT("時(2桁)")},
		{TEXT("tot-minute"),			TEXT("分")},
		{TEXT("tot-minute2"),			TEXT("分(2桁)")},
		{TEXT("tot-second"),			TEXT("秒")},
		{TEXT("tot-second2"),			TEXT("秒(2桁)")},
		{TEXT("tot-day-of-week"),		TEXT("曜日(漢字)")},
	};

	static const ParameterInfo EventDurationParams[] =
	{
		{TEXT("event-duration-hour"),	TEXT("時間")},
		{TEXT("event-duration-hour2"),	TEXT("時間(2桁)")},
		{TEXT("event-duration-min"),	TEXT("分")},
		{TEXT("event-duration-min2"),	TEXT("分(2桁)")},
		{TEXT("event-duration-sec"),	TEXT("秒")},
		{TEXT("event-duration-sec2"),	TEXT("秒(2桁)")},
	};

	static const ParameterInfo EventParams[] =
	{
		{TEXT("channel-name"),			TEXT("チャンネル名")},
		{TEXT("channel-no"),			TEXT("チャンネル番号")},
		{TEXT("channel-no2"),			TEXT("チャンネル番号(2桁)")},
		{TEXT("channel-no3"),			TEXT("チャンネル番号(3桁)")},
		{TEXT("event-name"),			TEXT("番組名")},
		{TEXT("event-title"),			TEXT("番組タイトル")},
		{TEXT("event-mark"),			TEXT("番組情報マーク")},
		{TEXT("event-id"),				TEXT("イベントID")},
		{TEXT("service-name"),			TEXT("サービス名")},
		{TEXT("service-id"),			TEXT("サービスID")},
		{TEXT("tuner-filename"),		TEXT("チューナーファイル名")},
		{TEXT("tuner-name"),			TEXT("チューナー名")},
	};

	static const ParameterInfo SeparatorParams[] =
	{
		{TEXT("sep-hyphen"),			TEXT("-(ハイフン)区切り")},
		{TEXT("sep-slash"),				TEXT("/(スラッシュ)区切り")},
		{TEXT("sep-backslash"),			TEXT("\\(バックスラッシュ)区切り")},
	};

	static const struct {
		LPCTSTR pszText;
		const ParameterInfo *pList;
		int ListLength;
		unsigned int Flags;
	} GroupList[] = {
		{TEXT("現在日時"),		DateTimeParams,			lengthof(DateTimeParams),		FLAG_NO_CURRENT_TIME},
		{TEXT("番組開始日時"),	StartTimeParams,		lengthof(StartTimeParams),		0},
		{TEXT("番組終了日時"),	EndTimeParams,			lengthof(EndTimeParams),		0},
		{TEXT("TOT日時"),		TotTimeParams,			lengthof(TotTimeParams),		FLAG_NO_TOT_TIME},
		{TEXT("番組の長さ"),	EventDurationParams,	lengthof(EventDurationParams),	0},
		{TEXT("区切り"),		SeparatorParams,		lengthof(SeparatorParams),		FLAG_NO_SEPARATOR},
		{nullptr,				EventParams,			lengthof(EventParams),			0},
	};

	for (int i=0;i<lengthof(GroupList);i++) {
		if ((GroupList[i].Flags & m_Flags)==0) {
			pList->push_back(ParameterGroup());
			ParameterGroup &Group=pList->back();

			if (GroupList[i].pszText!=nullptr)
				Group.Text=GroupList[i].pszText;
			Group.ParameterList.insert(
				Group.ParameterList.begin(),
				GroupList[i].pList,
				GroupList[i].pList+GroupList[i].ListLength);
		}
	}

	CBasicVariableStringMap::GetParameterList(pList);

	return true;
}


void CEventVariableStringMap::SetSampleEventInfo()
{
	GetSampleEventInfo(&m_EventInfo);
}


void CEventVariableStringMap::SetCurrentTime(const SYSTEMTIME *pTime)
{
	if (pTime!=nullptr) {
		m_fCurrentTimeSet=true;
		m_CurrentTime=*pTime;
	} else {
		m_fCurrentTimeSet=false;
	}
}


bool CEventVariableStringMap::GetSampleEventInfo(EventInfo *pInfo)
{
	if (pInfo==nullptr)
		return false;

	SYSTEMTIME st;

	::GetLocalTime(&st);
	st.wMilliseconds=0;

	pInfo->Channel.SetSpace(0);
	pInfo->Channel.SetChannelIndex(8);
	pInfo->Channel.SetChannelNo(13);
	pInfo->Channel.SetName(TEXT("アフリカ中央テレビ"));
	pInfo->Channel.SetNetworkID(0x1234);
	pInfo->Channel.SetTransportStreamID(0x1234);
	pInfo->Channel.SetServiceID(123);
	pInfo->Channel.SetTunerName(TEXT("BonDriver_TV.dll"));
	pInfo->ServiceName=TEXT("アフテレ1");
	pInfo->TotTime=st;
	pInfo->Event.m_NetworkID=0x1234;
	pInfo->Event.m_TransportStreamID=0x1234;
	pInfo->Event.m_ServiceID=123;
	pInfo->Event.m_EventID=0xABCD;
	pInfo->Event.m_EventName=TEXT("[二][字]今日のニュース");
	pInfo->Event.m_EventText=TEXT("本日のニュースをお伝えします。");
	pInfo->Event.m_bValidStartTime=true;
	OffsetSystemTime(&st,5*TimeConsts::SYSTEMTIME_MINUTE);
	st.wMinute=st.wMinute/5*5;
	st.wSecond=0;
	pInfo->Event.m_StartTime=st;
	pInfo->Event.m_Duration=60*60;

	return true;
}


#define MAX_MARK_LENGTH 3

void CEventVariableStringMap::GetEventTitle(const String &EventName,String *pTitle)
{
	// 番組名から [字] のようなものを除去する

	pTitle->clear();

	String::size_type Next=0;

	while (Next<EventName.length()) {
		String::size_type Left=EventName.find(L'[',Next);
		if (Left==String::npos) {
			pTitle->append(EventName.substr(Next));
			break;
		}

		String::size_type Right=EventName.find(L']',Left+1);
		if (Right==String::npos) {
			pTitle->append(EventName.substr(Next));
			break;
		}

		if (Left>Next)
			pTitle->append(EventName.substr(Next,Left-Next));

		if (Right-Left+1>MAX_MARK_LENGTH+2)
			pTitle->append(EventName.substr(Left,Right-Left+1));

		Next=Right+1;
	}

	StringUtility::Trim(*pTitle,L" ");
}


void CEventVariableStringMap::GetEventMark(const String &EventName,String *pMarks)
{
	// 番組名から [字] のようなものを抽出する

	pMarks->clear();

	String::size_type Next=0;

	while (Next<EventName.length()) {
		String::size_type Left=EventName.find(L'[',Next);
		if (Left==String::npos)
			break;

		String::size_type Right=EventName.find(L']',Left+1);
		if (Right==String::npos)
			break;

		String::size_type Length=Right-Left+1;
		if (Length>2 && Length<=MAX_MARK_LENGTH+2)
			pMarks->append(EventName.substr(Left,Length));

		Next=Right+1;
	}
}


}
