#include <core/stdafx.h>
#include <core/smartview/AppointmentRecurrencePattern.h>
#include <core/smartview/SmartView.h>
#include <core/interpret/flags.h>
#include <core/mapi/extraPropTags.h>
#include <core/interpret/guid.h>

namespace smartview
{
	ExceptionInfo::ExceptionInfo(std::shared_ptr<binaryParser>& parser)
	{
		StartDateTime = parser->Get<DWORD>();
		EndDateTime = parser->Get<DWORD>();
		OriginalStartDate = parser->Get<DWORD>();
		OverrideFlags = parser->Get<WORD>();
		if (OverrideFlags & ARO_SUBJECT)
		{
			SubjectLength = parser->Get<WORD>();
			SubjectLength2 = parser->Get<WORD>();
			if (SubjectLength2 && SubjectLength2 + 1 == SubjectLength)
			{
				Subject.parse(parser, SubjectLength2);
			}
		}

		if (OverrideFlags & ARO_MEETINGTYPE)
		{
			MeetingType = parser->Get<DWORD>();
		}

		if (OverrideFlags & ARO_REMINDERDELTA)
		{
			ReminderDelta = parser->Get<DWORD>();
		}
		if (OverrideFlags & ARO_REMINDER)
		{
			ReminderSet = parser->Get<DWORD>();
		}

		if (OverrideFlags & ARO_LOCATION)
		{
			LocationLength = parser->Get<WORD>();
			LocationLength2 = parser->Get<WORD>();
			if (LocationLength2 && LocationLength2 + 1 == LocationLength)
			{
				Location.parse(parser, LocationLength2);
			}
		}

		if (OverrideFlags & ARO_BUSYSTATUS)
		{
			BusyStatus = parser->Get<DWORD>();
		}

		if (OverrideFlags & ARO_ATTACHMENT)
		{
			Attachment = parser->Get<DWORD>();
		}

		if (OverrideFlags & ARO_SUBTYPE)
		{
			SubType = parser->Get<DWORD>();
		}

		if (OverrideFlags & ARO_APPTCOLOR)
		{
			AppointmentColor = parser->Get<DWORD>();
		}
	}

	ExtendedException::ExtendedException(std::shared_ptr<binaryParser>& parser, DWORD writerVersion2, WORD flags)
	{
		if (writerVersion2 >= 0x0003009)
		{
			ChangeHighlight.ChangeHighlightSize = parser->Get<DWORD>();
			ChangeHighlight.ChangeHighlightValue = parser->Get<DWORD>();
			if (ChangeHighlight.ChangeHighlightSize > sizeof(DWORD))
			{
				ChangeHighlight.Reserved.parse(parser, ChangeHighlight.ChangeHighlightSize - sizeof(DWORD), _MaxBytes);
			}
		}

		ReservedBlockEE1Size = parser->Get<DWORD>();
		ReservedBlockEE1.parse(parser, ReservedBlockEE1Size, _MaxBytes);

		if (flags & ARO_SUBJECT || flags & ARO_LOCATION)
		{
			StartDateTime = parser->Get<DWORD>();
			EndDateTime = parser->Get<DWORD>();
			OriginalStartDate = parser->Get<DWORD>();
		}

		if (flags & ARO_SUBJECT)
		{
			WideCharSubjectLength = parser->Get<WORD>();
			if (WideCharSubjectLength)
			{
				WideCharSubject.parse(parser, WideCharSubjectLength);
			}
		}

		if (flags & ARO_LOCATION)
		{
			WideCharLocationLength = parser->Get<WORD>();
			if (WideCharLocationLength)
			{
				WideCharLocation.parse(parser, WideCharLocationLength);
			}
		}

		if (flags & ARO_SUBJECT || flags & ARO_LOCATION)
		{
			ReservedBlockEE2Size = parser->Get<DWORD>();
			ReservedBlockEE2.parse(parser, ReservedBlockEE2Size, _MaxBytes);
		}
	}

	void AppointmentRecurrencePattern::Parse()
	{
		m_RecurrencePattern.parse(m_Parser, false);

		m_ReaderVersion2 = m_Parser->Get<DWORD>();
		m_WriterVersion2 = m_Parser->Get<DWORD>();
		m_StartTimeOffset = m_Parser->Get<DWORD>();
		m_EndTimeOffset = m_Parser->Get<DWORD>();
		m_ExceptionCount = m_Parser->Get<WORD>();

		if (m_ExceptionCount && m_ExceptionCount == m_RecurrencePattern.m_ModifiedInstanceCount &&
			m_ExceptionCount < _MaxEntriesSmall)
		{
			m_ExceptionInfo.reserve(m_ExceptionCount);
			for (WORD i = 0; i < m_ExceptionCount; i++)
			{
				m_ExceptionInfo.emplace_back(std::make_shared<ExceptionInfo>(m_Parser));
			}
		}

		m_ReservedBlock1Size = m_Parser->Get<DWORD>();
		m_ReservedBlock1.parse(m_Parser, m_ReservedBlock1Size, _MaxBytes);

		if (m_ExceptionCount && m_ExceptionCount == m_RecurrencePattern.m_ModifiedInstanceCount &&
			m_ExceptionCount < _MaxEntriesSmall && !m_ExceptionInfo.empty())
		{
			for (WORD i = 0; i < m_ExceptionCount; i++)
			{
				m_ExtendedException.emplace_back(
					std::make_shared<ExtendedException>(m_Parser, m_WriterVersion2, m_ExceptionInfo[i]->OverrideFlags));
			}
		}

		m_ReservedBlock2Size = m_Parser->Get<DWORD>();
		m_ReservedBlock2.parse(m_Parser, m_ReservedBlock2Size, _MaxBytes);
	}

	void AppointmentRecurrencePattern::ParseBlocks()
	{
		setRoot(m_RecurrencePattern.getBlock());

		terminateBlock();
		auto arpBlock = block{};
		arpBlock.setText(L"Appointment Recurrence Pattern: \r\n");
		arpBlock.addBlock(m_ReaderVersion2, L"ReaderVersion2: 0x%1!08X!\r\n", m_ReaderVersion2.getData());
		arpBlock.addBlock(m_WriterVersion2, L"WriterVersion2: 0x%1!08X!\r\n", m_WriterVersion2.getData());
		arpBlock.addBlock(
			m_StartTimeOffset,
			L"StartTimeOffset: 0x%1!08X! = %1!d! = %2!ws!\r\n",
			m_StartTimeOffset.getData(),
			RTimeToString(m_StartTimeOffset).c_str());
		arpBlock.addBlock(
			m_EndTimeOffset,
			L"EndTimeOffset: 0x%1!08X! = %1!d! = %2!ws!\r\n",
			m_EndTimeOffset.getData(),
			RTimeToString(m_EndTimeOffset).c_str());

		auto exceptions = m_ExceptionCount;
		exceptions.setText(L"ExceptionCount: 0x%1!04X!\r\n", m_ExceptionCount.getData());

		if (!m_ExceptionInfo.empty())
		{
			for (WORD i = 0; i < m_ExceptionInfo.size(); i++)
			{
				auto exception = block{};
				exception.addBlock(
					m_ExceptionInfo[i]->StartDateTime,
					L"ExceptionInfo[%1!d!].StartDateTime: 0x%2!08X! = %3!ws!\r\n",
					i,
					m_ExceptionInfo[i]->StartDateTime.getData(),
					RTimeToString(m_ExceptionInfo[i]->StartDateTime).c_str());
				exception.addBlock(
					m_ExceptionInfo[i]->EndDateTime,
					L"ExceptionInfo[%1!d!].EndDateTime: 0x%2!08X! = %3!ws!\r\n",
					i,
					m_ExceptionInfo[i]->EndDateTime.getData(),
					RTimeToString(m_ExceptionInfo[i]->EndDateTime).c_str());
				exception.addBlock(
					m_ExceptionInfo[i]->OriginalStartDate,
					L"ExceptionInfo[%1!d!].OriginalStartDate: 0x%2!08X! = %3!ws!\r\n",
					i,
					m_ExceptionInfo[i]->OriginalStartDate.getData(),
					RTimeToString(m_ExceptionInfo[i]->OriginalStartDate).c_str());
				auto szOverrideFlags = flags::InterpretFlags(flagOverrideFlags, m_ExceptionInfo[i]->OverrideFlags);
				exception.addBlock(
					m_ExceptionInfo[i]->OverrideFlags,
					L"ExceptionInfo[%1!d!].OverrideFlags: 0x%2!04X! = %3!ws!\r\n",
					i,
					m_ExceptionInfo[i]->OverrideFlags.getData(),
					szOverrideFlags.c_str());

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_SUBJECT)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->SubjectLength,
						L"ExceptionInfo[%1!d!].SubjectLength: 0x%2!04X! = %2!d!\r\n",
						i,
						m_ExceptionInfo[i]->SubjectLength.getData());
					exception.addBlock(
						m_ExceptionInfo[i]->SubjectLength2,
						L"ExceptionInfo[%1!d!].SubjectLength2: 0x%2!04X! = %2!d!\r\n",
						i,
						m_ExceptionInfo[i]->SubjectLength2.getData());

					exception.addBlock(
						m_ExceptionInfo[i]->Subject,
						L"ExceptionInfo[%1!d!].Subject: \"%2!hs!\"\r\n",
						i,
						m_ExceptionInfo[i]->Subject.c_str());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_MEETINGTYPE)
				{
					auto szFlags = InterpretNumberAsStringNamedProp(
						m_ExceptionInfo[i]->MeetingType,
						dispidApptStateFlags,
						const_cast<LPGUID>(&guid::PSETID_Appointment));
					exception.addBlock(
						m_ExceptionInfo[i]->MeetingType,
						L"ExceptionInfo[%1!d!].MeetingType: 0x%2!08X! = %3!ws!\r\n",
						i,
						m_ExceptionInfo[i]->MeetingType.getData(),
						szFlags.c_str());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_REMINDERDELTA)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->ReminderDelta,
						L"ExceptionInfo[%1!d!].ReminderDelta: 0x%2!08X!\r\n",
						i,
						m_ExceptionInfo[i]->ReminderDelta.getData());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_REMINDER)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->ReminderSet,
						L"ExceptionInfo[%1!d!].ReminderSet: 0x%2!08X!\r\n",
						i,
						m_ExceptionInfo[i]->ReminderSet.getData());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_LOCATION)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->LocationLength,
						L"ExceptionInfo[%1!d!].LocationLength: 0x%2!04X! = %2!d!\r\n",
						i,
						m_ExceptionInfo[i]->LocationLength.getData());
					exception.addBlock(
						m_ExceptionInfo[i]->LocationLength2,
						L"ExceptionInfo[%1!d!].LocationLength2: 0x%2!04X! = %2!d!\r\n",
						i,
						m_ExceptionInfo[i]->LocationLength2.getData());
					exception.addBlock(
						m_ExceptionInfo[i]->Location,
						L"ExceptionInfo[%1!d!].Location: \"%2!hs!\"\r\n",
						i,
						m_ExceptionInfo[i]->Location.c_str());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_BUSYSTATUS)
				{
					auto szFlags = InterpretNumberAsStringNamedProp(
						m_ExceptionInfo[i]->BusyStatus,
						dispidBusyStatus,
						const_cast<LPGUID>(&guid::PSETID_Appointment));
					exception.addBlock(
						m_ExceptionInfo[i]->BusyStatus,
						L"ExceptionInfo[%1!d!].BusyStatus: 0x%2!08X! = %3!ws!\r\n",
						i,
						m_ExceptionInfo[i]->BusyStatus.getData(),
						szFlags.c_str());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_ATTACHMENT)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->Attachment,
						L"ExceptionInfo[%1!d!].Attachment: 0x%2!08X!\r\n",
						i,
						m_ExceptionInfo[i]->Attachment.getData());
				}

				if (m_ExceptionInfo[i]->OverrideFlags & ARO_SUBTYPE)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->SubType,
						L"ExceptionInfo[%1!d!].SubType: 0x%2!08X!\r\n",
						i,
						m_ExceptionInfo[i]->SubType.getData());
				}
				if (m_ExceptionInfo[i]->OverrideFlags & ARO_APPTCOLOR)
				{
					exception.addBlock(
						m_ExceptionInfo[i]->AppointmentColor,
						L"ExceptionInfo[%1!d!].AppointmentColor: 0x%2!08X!\r\n",
						i,
						m_ExceptionInfo[i]->AppointmentColor.getData());
				}

				exceptions.addBlock(exception);
			}
		}

		arpBlock.addBlock(exceptions);
		auto reservedBlock1 = m_ReservedBlock1Size;
		reservedBlock1.setText(L"ReservedBlock1Size: 0x%1!08X!", m_ReservedBlock1Size.getData());
		if (m_ReservedBlock1Size)
		{
			reservedBlock1.terminateBlock();
			reservedBlock1.addBlock(m_ReservedBlock1);
		}

		reservedBlock1.terminateBlock();
		arpBlock.addBlock(reservedBlock1);

		if (!m_ExtendedException.empty())
		{
			auto i = 0;
			for (const auto& ee : m_ExtendedException)
			{
				auto exception = block{};
				if (m_WriterVersion2 >= 0x00003009)
				{
					auto szFlags = InterpretNumberAsStringNamedProp(
						ee->ChangeHighlight.ChangeHighlightValue,
						dispidChangeHighlight,
						const_cast<LPGUID>(&guid::PSETID_Appointment));
					exception.addBlock(
						ee->ChangeHighlight.ChangeHighlightSize,
						L"ExtendedException[%1!d!].ChangeHighlight.ChangeHighlightSize: 0x%2!08X!\r\n",
						i,
						ee->ChangeHighlight.ChangeHighlightSize.getData());
					exception.addBlock(
						ee->ChangeHighlight.ChangeHighlightValue,
						L"ExtendedException[%1!d!].ChangeHighlight.ChangeHighlightValue: 0x%2!08X! = %3!ws!\r\n",
						i,
						ee->ChangeHighlight.ChangeHighlightValue.getData(),
						szFlags.c_str());

					if (ee->ChangeHighlight.ChangeHighlightSize > sizeof(DWORD))
					{
						exception.addHeader(L"ExtendedException[%1!d!].ChangeHighlight.Reserved:", i);

						exception.addBlock(ee->ChangeHighlight.Reserved);
					}
				}

				exception.addBlock(
					ee->ReservedBlockEE1Size,
					L"ExtendedException[%1!d!].ReservedBlockEE1Size: 0x%2!08X!\r\n",
					i,
					ee->ReservedBlockEE1Size.getData());
				if (!ee->ReservedBlockEE1.empty())
				{
					exception.addBlock(ee->ReservedBlockEE1);
				}

				if (i < m_ExceptionInfo.size())
				{
					if (m_ExceptionInfo[i]->OverrideFlags & ARO_SUBJECT ||
						m_ExceptionInfo[i]->OverrideFlags & ARO_LOCATION)
					{
						exception.addBlock(
							ee->StartDateTime,
							L"ExtendedException[%1!d!].StartDateTime: 0x%2!08X! = %3\r\n",
							i,
							ee->StartDateTime.getData(),
							RTimeToString(ee->StartDateTime).c_str());
						exception.addBlock(
							ee->EndDateTime,
							L"ExtendedException[%1!d!].EndDateTime: 0x%2!08X! = %3!ws!\r\n",
							i,
							ee->EndDateTime.getData(),
							RTimeToString(ee->EndDateTime).c_str());
						exception.addBlock(
							ee->OriginalStartDate,
							L"ExtendedException[%1!d!].OriginalStartDate: 0x%2!08X! = %3!ws!\r\n",
							i,
							ee->OriginalStartDate.getData(),
							RTimeToString(ee->OriginalStartDate).c_str());
					}

					if (m_ExceptionInfo[i]->OverrideFlags & ARO_SUBJECT)
					{
						exception.addBlock(
							ee->WideCharSubjectLength,
							L"ExtendedException[%1!d!].WideCharSubjectLength: 0x%2!08X! = %2!d!\r\n",
							i,
							ee->WideCharSubjectLength.getData());
						exception.addBlock(
							ee->WideCharSubject,
							L"ExtendedException[%1!d!].WideCharSubject: \"%2!ws!\"\r\n",
							i,
							ee->WideCharSubject.c_str());
					}

					if (m_ExceptionInfo[i]->OverrideFlags & ARO_LOCATION)
					{
						exception.addBlock(
							ee->WideCharLocationLength,
							L"ExtendedException[%1!d!].WideCharLocationLength: 0x%2!08X! = %2!d!\r\n",
							i,
							ee->WideCharLocationLength.getData());
						exception.addBlock(
							ee->WideCharLocation,
							L"ExtendedException[%1!d!].WideCharLocation: \"%2!ws!\"\r\n",
							i,
							ee->WideCharLocation.c_str());
					}
				}

				exception.addBlock(
					ee->ReservedBlockEE2Size,
					L"ExtendedException[%1!d!].ReservedBlockEE2Size: 0x%2!08X!\r\n",
					i,
					ee->ReservedBlockEE2Size.getData());
				if (ee->ReservedBlockEE2Size)
				{
					exception.addBlock(ee->ReservedBlockEE2);
				}

				arpBlock.addBlock(exception);
				i++;
			}
		}

		auto reservedBlock2 = m_ReservedBlock2Size;
		reservedBlock2.setText(L"ReservedBlock2Size: 0x%1!08X!", m_ReservedBlock2Size.getData());
		if (m_ReservedBlock2Size)
		{
			reservedBlock2.terminateBlock();
			reservedBlock2.addBlock(m_ReservedBlock2);
		}

		reservedBlock2.terminateBlock();
		arpBlock.addBlock(reservedBlock2);
		addBlock(arpBlock);
	}
} // namespace smartview