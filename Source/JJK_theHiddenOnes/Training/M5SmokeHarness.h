#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "M5SmokeHarness.generated.h"

/** 显式 -M5SmokeTest 开发包验收；普通运行不创建，Shipping 不执行。 */
UCLASS()
class UM5SmokeHarness : public UActorComponent
{
 GENERATED_BODY()
public:
 UM5SmokeHarness();
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick) override;
private:
 void Check(bool Condition,const TCHAR* Name);
 void Complete();
 int32 Stage=0, Checks=0, Failures=0;
 double StageTime=0., Started=0.;
 float InitialHealth=0.;
 TArray<FString> Results;
};
