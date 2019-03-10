#pragma once
#include <core/smartview/SmartViewParser.h>
#include <core/smartview/block/blockBytes.h>

namespace smartview
{
	class XID : public SmartViewParser
	{
	private:
		void Parse() override;
		void ParseBlocks() override;

		blockT<GUID> m_NamespaceGuid;
		size_t m_cbLocalId{};
		blockBytes m_LocalID;
	};
} // namespace smartview