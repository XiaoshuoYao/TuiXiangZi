// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * DataTable query and edit tools:
 * get_datatable_schema, get_datatable_rows, list_datatable_row_names,
 * validate_datatable_rows, create_datatable, upsert_datatable_rows,
 * delete_datatable_rows, replace_datatable_rows, rename_datatable_row,
 * save_datatable.
 */
namespace MCPTool_DataTable
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
