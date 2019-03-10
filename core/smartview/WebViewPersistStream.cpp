#include <core/stdafx.h>
#include <core/smartview/WebViewPersistStream.h>
#include <core/interpret/flags.h>
#include <core/mapi/extraPropTags.h>
#include <core/utility/strings.h>

namespace smartview
{
	WebViewPersist::WebViewPersist(std::shared_ptr<binaryParser>& parser)
	{
		dwVersion = parser->Get<DWORD>();
		dwType = parser->Get<DWORD>();
		dwFlags = parser->Get<DWORD>();
		dwUnused.parse(parser, 7 * sizeof(DWORD));
		cbData = parser->Get<DWORD>();
		lpData.parse(parser, cbData, _MaxBytes);
	}

	void WebViewPersistStream::Parse()
	{
		// Run through the parser once to count the number of web view structs
		for (;;)
		{
			// Must have at least 2 bytes left to have another struct
			if (m_Parser->RemainingBytes() < sizeof(DWORD) * 11) break;
			m_Parser->advance(sizeof(DWORD) * 10);
			const auto cbData = m_Parser->Get<DWORD>();

			// Must have at least cbData bytes left to be a valid flag
			if (m_Parser->RemainingBytes() < cbData) break;

			m_Parser->advance(cbData);
			m_cWebViews++;
		}

		// Now we parse for real
		m_Parser->rewind();

		const auto cWebViews = m_cWebViews;
		if (cWebViews && cWebViews < _MaxEntriesSmall)
		{
			m_lpWebViews.reserve(cWebViews);
			for (ULONG i = 0; i < cWebViews; i++)
			{
				m_lpWebViews.emplace_back(std::make_shared<WebViewPersist>(m_Parser));
			}
		}
	}

	void WebViewPersistStream::ParseBlocks()
	{
		setRoot(L"Web View Persistence Object Stream\r\n");
		addHeader(L"cWebViews = %1!d!", m_cWebViews);
		auto i = 0;
		for (const auto& view : m_lpWebViews)
		{
			terminateBlock();
			addBlankLine();

			addHeader(L"Web View %1!d!\r\n", i);
			addBlock(
				view->dwVersion,
				L"dwVersion = 0x%1!08X! = %2!ws!\r\n",
				view->dwVersion.getData(),
				flags::InterpretFlags(flagWebViewVersion, view->dwVersion).c_str());
			addBlock(
				view->dwType,
				L"dwType = 0x%1!08X! = %2!ws!\r\n",
				view->dwType.getData(),
				flags::InterpretFlags(flagWebViewType, view->dwType).c_str());
			addBlock(
				view->dwFlags,
				L"dwFlags = 0x%1!08X! = %2!ws!\r\n",
				view->dwFlags.getData(),
				flags::InterpretFlags(flagWebViewFlags, view->dwFlags).c_str());
			addHeader(L"dwUnused = ");

			addBlock(view->dwUnused);

			terminateBlock();
			addBlock(view->cbData, L"cbData = 0x%1!08X!\r\n", view->cbData.getData());

			switch (view->dwType)
			{
			case WEBVIEWURL:
			{
				addHeader(L"wzURL = ");
				addBlock(view->lpData, strings::BinToTextStringW(view->lpData, false));
				break;
			}
			default:
				addHeader(L"lpData = ");
				addBlock(view->lpData);
				break;
			}

			i++;
		}
	}
} // namespace smartview