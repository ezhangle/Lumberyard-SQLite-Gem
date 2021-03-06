
#include "StdAfx.h"
#include "SQLiteSystemComponent.h"

namespace SQLite {
	////////////////////////////////////////////////////////////////////////
	// Component Implementation
	SQLiteSystemComponent::SQLiteSystemComponent() {
		this->m_dbPath = ":memory:";
		this->m_OpenType = OPEN;
		this->m_openv2_flags = 0;
		this->m_openv2_zvfs = "";
	}

	void SQLiteSystemComponent::Reflect(AZ::ReflectContext* context) {
		if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context)) {
			serialize->Class<SQLiteSystemComponent, AZ::Component>()
				->Version(1)
				->Field("dbPath", &SQLiteSystemComponent::m_dbPath)
				->Field("OpenType", &SQLiteSystemComponent::m_OpenType)
				->Field("Openv2_flags", &SQLiteSystemComponent::m_openv2_flags)
				->Field("Openv2_zvfs", &SQLiteSystemComponent::m_openv2_zvfs);

			if (AZ::EditContext* ec = serialize->GetEditContext()) {
				ec->Class<SQLiteSystemComponent>("SQLite", "SQLite database component.")
					->ClassElement(AZ::Edit::ClassElements::EditorData, "")
					->Attribute(AZ::Edit::Attributes::Category, "Database")
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
					->Attribute(AZ::Edit::Attributes::AutoExpand, true)
					->DataElement(0, &SQLiteSystemComponent::m_dbPath, "Database", "Database Path")
					->DataElement(AZ::Edit::UIHandlers::ComboBox, &SQLiteSystemComponent::m_OpenType, "Open Type", "Which open function to use.")
						->EnumAttribute(OpenType::OPEN, "Open")
						->EnumAttribute(OpenType::OPEN16, "Open16")
						->EnumAttribute(OpenType::OPENV2, "Openv2")
					->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
					->DataElement(0, &SQLiteSystemComponent::m_openv2_flags, "Open V2 Flags", "Flags to use for Open V2.")
						->Attribute(AZ::Edit::Attributes::Visibility, &SQLiteSystemComponent::IsOpenTypeV2)
					->DataElement(0, &SQLiteSystemComponent::m_openv2_zvfs, "Open V2 VFS", "Virtual filesystem to use for Open V2.")
						->Attribute(AZ::Edit::Attributes::Visibility, &SQLiteSystemComponent::IsOpenTypeV2);
			}
		}

		AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
		if (behaviorContext) {
			#define SQLITE_EVENT(name) ->Event(#name, &SQLiteRequestBus::Events::##name##)
			#define SQLITE_EVENTLUA(name) ->Event(#name, &SQLiteRequestBus::Events::##name##Lua)
			behaviorContext->EBus<SQLiteRequestBus>("SQLiteBus")
				->Handler<LuaHandler>()
				SQLITE_EVENTLUA(Exec)
				SQLITE_EVENTLUA(ExecTo)
				SQLITE_EVENT(GetConnection)
				//SQLITE_EVENTLUA(GetSysConnection)
				;
			behaviorContext->Class<SQLiteSystemComponent>("SQLiteLY")
				->Method("GetSysConnection", &SQLiteSystemComponent::GetSysConnectionLua, nullptr, "");
			#undef SQLITE_EVENTLUA
			#undef SQLITE_EVENT

			//behaviorContext->EBus<SQLiteLuaRequests>("");
		}
	}

	void SQLiteSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided) {
		provided.push_back(AZ_CRC("SQLiteLYService"));
	}

	void SQLiteSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible) {
		incompatible.push_back(AZ_CRC("SQLiteLYService"));
	}

	void SQLiteSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required) {
		(void)required;// .push_back(AZ_CRC("SQLite3Service"));
	}

	void SQLiteSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent) {
		(void)dependent;
		//dependent.push_back(AZ_CRC("SQLite3Service"));
	}

	void SQLiteSystemComponent::Init() {
		this->m_pDB = new SQLite3::SQLiteDB();
	}

	void SQLiteSystemComponent::Activate() {
		AZ_Printf("SQLiteLY", "%s - Opening Database: %s", this->GetEntityId().ToString().c_str(), this->m_dbPath.c_str());

		switch (this->m_OpenType) {
		case OPEN:
			this->m_pDB->Open(this->m_dbPath.c_str());
			break;
		case OPEN16:
			this->m_pDB->Open16(this->m_dbPath.c_str());
			break;
		case OPENV2:
			this->m_pDB->Open_v2(this->m_dbPath.c_str(), m_openv2_flags, m_openv2_zvfs.c_str());
			break;
		}
		SQLiteRequestBus::Handler::BusConnect(this->GetEntityId());
	}

	void SQLiteSystemComponent::Deactivate() {
		SQLiteRequestBus::Handler::BusDisconnect();
		AZ_Printf("SQLiteLY", "%s - Closing Database", this->GetEntityId().ToString().c_str());
		this->m_pDB->Close();
		delete this->m_pDB;
	}
	////////////////////////////////////////////////////////////////////////

	SQLite3::SQLiteDB * SQLiteSystemComponent::GetConnection() {
		if (this->GetEntityId() == this->m_pDB->m_entityid) return this->m_pDB;
		if (this->GetEntityId() == AZ::EntityId(0)) return this->m_pDB;
		if (this->m_pDB) return new SQLite3::SQLiteDB(this->m_pDB, this->GetEntityId()); //creates an alias to an entity's db?
		return nullptr;
	}

	////////////////////////////////////////////////////////////////////////
	// Lua Implementation
	int SQLiteSystemComponent::ExecLua(const char *sql, void * cbarg) {
		typedef struct {
			void* cbarg;
			AZ::EntityId id;
		} ARGS;

		ARGS args;
		args.cbarg = cbarg;
		args.id = this->GetEntityId();

		auto callback = [](void* cbarg, int argc, char **argv, char **azColName) -> int {
			int ret;
			ARGS* args = (ARGS*)cbarg;

			//////////////////////////
			for (int i = 0; i < argc; i++) {
				AZ_Printf("SQLiteLY", "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
			}

			AZStd::vector<AZStd::string> argvVec(argv, argc + argv);
			AZStd::vector<AZStd::string> azColNameVec(azColName, azColName+argc);
			//////////////////////////

			//SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argv, azColName);
			SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argvVec, azColNameVec);
			
			return ret;
		};

		//char * errmsg = 0;

		return this->m_pDB->Exec(
			sql,
			callback,
			&args,
			nullptr
		);
	}

	int SQLiteSystemComponent::ExecToLua(AZ::EntityId id, const char *sql, void * cbarg) {
		typedef struct {
			void* cbarg;
			AZ::EntityId id;
		} ARGS;

		ARGS args;
		args.cbarg = cbarg;
		args.id = id;

		auto callback = [](void* cbarg, int argc, char **argv, char **azColName) -> int {
			int ret;
			ARGS* args = (ARGS*)cbarg;

			//////////////////////////
			for (int i = 0; i < argc; i++) {
				AZ_Printf("SQLiteLY", "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
			}

			AZStd::vector<AZStd::string> argvVec(argv, argc + argv);
			AZStd::vector<AZStd::string> azColNameVec(azColName, azColName + argc);
			//////////////////////////

			//SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argv, azColName);
			SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argvVec, azColNameVec);

			return ret;
		};

		//char * errmsg = 0;

		return this->m_pDB->Exec(
			sql,
			callback,
			&args,
			nullptr
		);
	}

	int SQLiteSystemComponent::LuaHandler::SqlCallback(void * cbargs, int argc, AZStd::vector<AZStd::string> argv, AZStd::vector<AZStd::string> azColName){
	//int SQLiteSystemComponent::LuaHandler::SqlCallback(void * cbargs, int argc, char **argv, char **azColName) {
		int ret;
		CallResult(ret, FN_SqlCallback, cbargs, argc, argv, azColName);
		return ret;
	}

	/*namespace SQLite3 {
		void SQLite3Constructor(SQLite * thisPtr, AZ::ScriptDataContext& dc) {
			if (dc.GetNumArguments() == 1) {
				const char * path;
				dc.ReadArg(0, path);
				sqlite3_open(path, &thisPtr->ptr);
			}
		}
	}*/
	////////////////////////////////////////////////////////////////////////

	SQLite3::SQLiteDB * SQLiteSystemComponent::GetSysConnectionLua() {
		SQLite3::SQLiteDB * ret = nullptr;
		SQLITE_BUS(ret, AZ::EntityId(0), GetConnection);
		return ret;
	}
}
