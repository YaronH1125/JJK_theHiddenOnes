// Copyright Epic Games, Inc. All Rights Reserved.

#include "JJK_theHiddenOnes.h"
#include "Modules/ModuleManager.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagsManager.h"
#include "GameplayTaskResource.h"

// M0 基线：强制引用 GAS 符号，防止链接器丢弃未使用的模块依赖，
// 用于核验 Build.cs 中 GAS 依赖真实可用；M2 落地实际能力代码后移除。
void* const GGasLinkProbe[] = {
	(void*)&UAbilitySystemComponent::StaticClass,
	(void*)&UGameplayTagsManager::Get,
	(void*)&UAbilityTask::StaticClass,
	(void*)&UGameplayTaskResource::StaticClass,
};

class FJJK_theHiddenOnesModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		// 读取探针，保证 GAS 导入项不被 /OPT:REF 裁剪
		volatile void* ProbeRef = GGasLinkProbe[0];
		(void)ProbeRef;
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FJJK_theHiddenOnesModule, JJK_theHiddenOnes, "JJK_theHiddenOnes" );

DEFINE_LOG_CATEGORY(LogJJK_theHiddenOnes)