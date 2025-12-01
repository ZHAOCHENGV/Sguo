// 📄 文件：Source/Sguo/Private/Player/SG_PlayerController.cpp
// 🔧 修改 - 低耦合计谋卡处理实现

#include "Player/SG_PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Debug/SG_LogCategories.h"
#include "Engine/LocalPlayer.h"
#include "CardsAndUnits/SG_CardDeckComponent.h"
#include "UIHud/SG_CardHandWidget.h"
#include "Blueprint/UserWidget.h"
#include "Actors/SG_PlacementPreview.h"
#include "Data/SG_CardDataBase.h"
#include "Data/SG_CharacterCardData.h"
#include "Data/SG_StrategyCardData.h"
#include "Units/SG_UnitsBase.h"
#include "Player/SG_Player.h"
#include "Buildings/SG_MainCityBase.h"
#include "Kismet/GameplayStatics.h"
// ✨ 新增 - 计谋效果基类
#include "Strategies/SG_StrategyEffectBase.h"
#include "Strategies/SG_StrategyEffect_RollingLog.h"  // ✨ 新增

ASG_PlayerController::ASG_PlayerController()
{
	CardDeckComponent = CreateDefaultSubobject<USG_CardDeckComponent>(TEXT("CardDeckComponent"));
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ASG_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
		}
	}
	
	if (CardHandWidgetClass && CardDeckComponent)
	{
		CardHandWidget = CreateWidget<USG_CardHandWidget>(this, CardHandWidgetClass);
		if (CardHandWidget)
		{
			CardHandWidget->InitializeCardHand(CardDeckComponent);
			CardHandWidget->AddToViewport();
		}
	}
	
	if (CardDeckComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("PlayerController 准备初始化卡组..."));
		CardDeckComponent->OnSelectionChanged.AddDynamic(this, &ASG_PlayerController::OnCardSelectionChanged);
		CardDeckComponent->InitializeDeck();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ CardDeckComponent 不存在！"));
	}
	
	if (GetPawn())
	{
		UE_LOG(LogTemp, Log, TEXT("Pawn 已就绪，立即绑定输入事件"));
		BindPawnInputEvents();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ Pawn 尚未就绪，等待 OnPossess 回调"));
	}
	
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void ASG_PlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ✨ 新增 - 根据当前模式更新预览
	if (CurrentPlacementMode == ESGPlacementMode::StrategyTarget && ActiveStrategyEffect)
	{
		// 更新计谋效果预览位置
		FVector MouseLocation;
		if (GetMouseGroundLocation(MouseLocation))
		{
			ActiveStrategyEffect->UpdateTargetLocation(MouseLocation);
		}
	}
	// CardPlacement 模式由 PlacementPreview Actor 自己的 Tick 处理
}

void ASG_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UE_LOG(LogTemp, Log, TEXT("SetupInputComponent 被调用"));
}

void ASG_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	UE_LOG(LogTemp, Log, TEXT("========== OnPossess 被调用 =========="));
	UE_LOG(LogTemp, Log, TEXT("Pawn: %s"), InPawn ? *InPawn->GetName() : TEXT("nullptr"));
	
	BindPawnInputEvents();
	
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void ASG_PlayerController::BindPawnInputEvents()
{
	if (bPawnInputBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pawn 输入事件已绑定，跳过"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("========== 绑定 Pawn 输入事件 =========="));
	
	ASG_Player* PlayerPawn = Cast<ASG_Player>(GetPawn());
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 未找到 PlayerPawn！"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("找到 PlayerPawn: %s"), *PlayerPawn->GetName());
	
	PlayerPawn->OnConfirmInput.AddDynamic(this, &ASG_PlayerController::OnConfirmInput);
	UE_LOG(LogTemp, Log, TEXT("  ✓ 已绑定确认输入（左键）"));
	
	PlayerPawn->OnCancelInput.AddDynamic(this, &ASG_PlayerController::OnCancelInput);
	UE_LOG(LogTemp, Log, TEXT("  ✓ 已绑定取消输入（右键）"));
	
	bPawnInputBound = true;
	
	UE_LOG(LogTemp, Log, TEXT("✓ Pawn 输入事件绑定完成"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

USG_CardDeckComponent* ASG_PlayerController::GetCardDeckComponent() const
{
	return CardDeckComponent;
}

// 🔧 修改 - StartCardPlacement 使用通用计谋卡处理
void ASG_PlayerController::StartCardPlacement(USG_CardDataBase* CardData, const FGuid& CardInstanceId)
{
	if (!CardData)
	{
		UE_LOG(LogTemp, Error, TEXT("StartCardPlacement 失败：CardData 为空"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("========== 开始放置卡牌：%s =========="), *CardData->CardName.ToString());

	// 取消之前的任何放置模式
	if (CurrentPlacementMode != ESGPlacementMode::None)
	{
		CancelPlacement();
	}

	// ✨ 新增 - 检查是否是计谋卡
	USG_StrategyCardData* StrategyCard = Cast<USG_StrategyCardData>(CardData);
	if (StrategyCard)
	{
		// 检查是否需要目标选择
		if (!DoesCardRequirePreview(CardData))
		{
			// 全局效果，直接使用
			UE_LOG(LogSGGameplay, Log, TEXT("  全局效果卡牌，直接使用"));
			UseStrategyCardDirectly(StrategyCard, CardInstanceId);
			
			if (CardDeckComponent)
			{
				CardDeckComponent->SelectCard(FGuid());
			}
			return;
		}
		else
		{
			// 需要目标选择的计谋卡
			UE_LOG(LogSGGameplay, Log, TEXT("  计谋卡需要目标选择"));
			StartStrategyTargetSelection(StrategyCard, CardInstanceId);
			return;
		}
	}

	// 角色卡，使用普通放置预览
	if (!PlacementPreviewClass)
	{
		UE_LOG(LogTemp, Error, TEXT("StartCardPlacement 失败：PlacementPreviewClass 未设置"));
		return;
	}

	CurrentSelectedCardData = CardData;
	CurrentSelectedCardInstanceId = CardInstanceId;
	CurrentPlacementMode = ESGPlacementMode::CardPlacement;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetPawn();

	CurrentPreviewActor = GetWorld()->SpawnActor<ASG_PlacementPreview>(
		PlacementPreviewClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (CurrentPreviewActor)
	{
		CurrentPreviewActor->InitializePreview(CardData, this);
		UE_LOG(LogTemp, Log, TEXT("✓ 预览 Actor 已生成"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 预览 Actor 生成失败"));
		CurrentPlacementMode = ESGPlacementMode::None;
	}
}

// 🔧 修改 - ConfirmPlacement 根据模式处理
void ASG_PlayerController::ConfirmPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("========== 确认放置 =========="));

	switch (CurrentPlacementMode)
	{
	case ESGPlacementMode::StrategyTarget:
		// 计谋卡目标选择模式
		ConfirmStrategyTarget();
		return;

	case ESGPlacementMode::CardPlacement:
		// 普通卡牌放置模式，继续原有逻辑
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("ConfirmPlacement 失败：无放置模式"));
		return;
	}

	// 普通卡牌放置逻辑
	if (!CurrentPreviewActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfirmPlacement 失败：无预览 Actor"));
		return;
	}

	if (!CurrentPreviewActor->CanPlaceAtCurrentLocation())
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfirmPlacement 失败：当前位置不可放置"));
		return;
	}

	if (!CurrentSelectedCardData)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfirmPlacement 失败：卡牌数据为空"));
		CancelPlacement();
		return;
	}

	FVector UnitSpawnLocation = CurrentPreviewActor->GetPreviewLocation();
	FRotator UnitSpawnRotation = CalculateUnitSpawnRotation(UnitSpawnLocation);

	UE_LOG(LogSGGameplay, Log, TEXT("放置位置：%s"), *UnitSpawnLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("放置旋转：%s"), *UnitSpawnRotation.ToString());

	// 生成单位
	SpawnUnitFromCard(CurrentSelectedCardData, UnitSpawnLocation, UnitSpawnRotation);

	// 使用卡牌
	if (CardDeckComponent)
	{
		bool bSuccess = CardDeckComponent->UseCard(CurrentSelectedCardInstanceId);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ 卡牌使用成功，进入冷却"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ 卡牌使用失败"));
		}
	}

	// 清理
	if (CurrentPreviewActor)
	{
		CurrentPreviewActor->Destroy();
		CurrentPreviewActor = nullptr;
	}

	CurrentSelectedCardData = nullptr;
	CurrentSelectedCardInstanceId.Invalidate();
	CurrentPlacementMode = ESGPlacementMode::None;

	UE_LOG(LogTemp, Log, TEXT("✓ 放置完成"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

// 🔧 修改 - CancelPlacement 根据模式处理
void ASG_PlayerController::CancelPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("========== 取消放置 =========="));

	// 1. 如果是计谋模式，转交给专用函数（该函数内部已正确处理）
	if (CurrentPlacementMode == ESGPlacementMode::StrategyTarget)
	{
		CancelStrategyTargetSelection();
		return;
	}

	// 2. 如果当前本来就没在放置，直接返回
	if (CurrentPlacementMode == ESGPlacementMode::None)
	{
		return;
	}

	// 3. 【关键修复】先重置状态，再执行可能会触发回调的操作
	// 保存需要清理的变量
	FGuid InstanceIdToDeselect = CurrentSelectedCardInstanceId;
	
	// 立即重置状态，打断 SelectCard -> OnSelectionChanged -> CancelPlacement 的闭环
	CurrentSelectedCardData = nullptr;
	CurrentSelectedCardInstanceId.Invalidate();
	CurrentPlacementMode = ESGPlacementMode::None;

	// 4. 销毁预览 Actor
	if (CurrentPreviewActor)
	{
		CurrentPreviewActor->Destroy();
		CurrentPreviewActor = nullptr;
		UE_LOG(LogTemp, Log, TEXT("✓ 预览 Actor 已销毁"));
	}

	// 5. 调用外部组件方法（这会触发 OnSelectionChanged，但此时 Mode 已经是 None，不会导致递归）
	if (CardDeckComponent && InstanceIdToDeselect.IsValid())
	{
		CardDeckComponent->SelectCard(FGuid());
		UE_LOG(LogTemp, Log, TEXT("✓ 已取消选中卡牌"));
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

// ========== ✨ 新增 - 通用计谋卡接口实现 ==========

bool ASG_PlayerController::StartStrategyTargetSelection(USG_StrategyCardData* StrategyCardData, const FGuid& CardInstanceId)
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 开始计谋目标选择 =========="));

	if (!StrategyCardData)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 计谋卡数据为空"));
		return false;
	}

	// 检查效果类是否设置
	if (!StrategyCardData->EffectActorClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ EffectActorClass 未设置！"));
		return false;
	}

	// 获取鼠标初始位置
	FVector InitialLocation;
	if (!GetMouseGroundLocation(InitialLocation))
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法获取鼠标位置，使用原点"));
		InitialLocation = FVector::ZeroVector;
	}

	// 生成效果 Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetPawn();

	ActiveStrategyEffect = GetWorld()->SpawnActor<ASG_StrategyEffectBase>(
		StrategyCardData->EffectActorClass,
		InitialLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (!ActiveStrategyEffect)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 效果 Actor 生成失败"));
		return false;
	}

	// 初始化效果
	ActiveStrategyEffect->InitializeEffect(
		StrategyCardData,
		GetPawn(),
		InitialLocation
	);

	// 绑定完成回调
	ActiveStrategyEffect->OnEffectFinished.AddDynamic(this, &ASG_PlayerController::OnStrategyEffectFinished);

	// 开始目标选择（效果类自己负责预览显示）
	if (!ActiveStrategyEffect->StartTargetSelection())
	{
		// 开始失败（可能是条件不满足）
		FText Reason = ActiveStrategyEffect->GetCannotExecuteReason();
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法开始目标选择：%s"), *Reason.ToString());
		
		// 清理
		ActiveStrategyEffect->Destroy();
		ActiveStrategyEffect = nullptr;
		
		// 取消卡牌选中
		if (CardDeckComponent)
		{
			CardDeckComponent->SelectCard(FGuid());
		}
		
		return false;
	}

	// 保存卡牌实例 ID
	StrategyCardInstanceId = CardInstanceId;

	// 设置当前模式
	CurrentPlacementMode = ESGPlacementMode::StrategyTarget;

	UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 计谋目标选择已开始"));
	UE_LOG(LogSGGameplay, Log, TEXT("    效果类：%s"), *StrategyCardData->EffectActorClass->GetName());
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));

	return true;
}

bool ASG_PlayerController::ConfirmStrategyTarget()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 确认计谋目标 =========="));

	if (CurrentPlacementMode != ESGPlacementMode::StrategyTarget)
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 当前不在计谋目标选择模式"));
		return false;
	}

	if (!ActiveStrategyEffect)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 效果 Actor 为空"));
		CancelStrategyTargetSelection();
		return false;
	}

	// 🔧 修改 - 先保存需要的数据
	FGuid CardIdToUse = StrategyCardInstanceId;
	
	// 调用效果的确认方法（效果类自己负责验证和执行）
	bool bSuccess = ActiveStrategyEffect->ConfirmTarget();

	if (bSuccess)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 计谋目标确认成功"));

		// 🔧 修改 - 先清理状态，防止 UseCard 触发的 OnSelectionChanged 回调导致取消
		ActiveStrategyEffect = nullptr;
		StrategyCardInstanceId.Invalidate();
		CurrentPlacementMode = ESGPlacementMode::None;

		// 使用卡牌（这会触发 OnSelectionChanged，但此时 CurrentPlacementMode 已经是 None）
		if (CardDeckComponent && CardIdToUse.IsValid())
		{
			bool bCardUsed = CardDeckComponent->UseCard(CardIdToUse);
			if (bCardUsed)
			{
				UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 卡牌使用成功，进入冷却"));
			}
			else
			{
				UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 卡牌使用失败"));
			}
		}
	}
	else
	{
		UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 计谋目标确认失败"));
	}

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
	return bSuccess;
}

void ASG_PlayerController::CancelStrategyTargetSelection()
{
	UE_LOG(LogSGGameplay, Log, TEXT("========== 取消计谋目标选择 =========="));

	// 取消效果
	if (ActiveStrategyEffect)
	{
		// 解绑回调（防止销毁时触发）
		ActiveStrategyEffect->OnEffectFinished.RemoveDynamic(this, &ASG_PlayerController::OnStrategyEffectFinished);
		ActiveStrategyEffect->CancelEffect();
		ActiveStrategyEffect = nullptr;
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 效果 Actor 已取消"));
	}

	// 清理状态
	StrategyCardInstanceId.Invalidate();
	CurrentPlacementMode = ESGPlacementMode::None;

	// 取消卡牌选中
	if (CardDeckComponent)
	{
		CardDeckComponent->SelectCard(FGuid());
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 已取消选中卡牌"));
	}

	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_PlayerController::UseStrategyCardDirectly(USG_StrategyCardData* StrategyCardData, const FGuid& CardInstanceId)
{
	if (!StrategyCardData)
	{
		UE_LOG(LogTemp, Error, TEXT("UseStrategyCardDirectly 失败：StrategyCardData 为空"));
		return;
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("========== 直接使用计谋卡：%s =========="), 
		*StrategyCardData->CardName.ToString());
    
	// 检查效果类是否设置
	if (!StrategyCardData->EffectActorClass)
	{
		// 如果没有效果类，尝试使用纯 GE 模式
		if (StrategyCardData->GameplayEffectClass)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  使用纯 GE 模式"));
			
			// 获取施放者阵营
			FGameplayTag PlayerFactionTag = FGameplayTag::RequestGameplayTag(FName("Unit.Faction.Player"), false);
			
			// 获取所有友方单位
			TArray<AActor*> FriendlyUnits;
			TArray<AActor*> AllUnits;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_UnitsBase::StaticClass(), AllUnits);
			
			for (AActor* Actor : AllUnits)
			{
				ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
				if (Unit && !Unit->bIsDead && Unit->FactionTag.MatchesTag(PlayerFactionTag))
				{
					FriendlyUnits.Add(Unit);
				}
			}
			
			UE_LOG(LogSGGameplay, Log, TEXT("  找到 %d 个友方单位"), FriendlyUnits.Num());
			
			FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"), false);
			
			int32 SuccessCount = 0;
			for (AActor* Actor : FriendlyUnits)
			{
				ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
				if (!Unit) continue;
				
				UAbilitySystemComponent* UnitASC = Unit->GetAbilitySystemComponent();
				if (!UnitASC) continue;
				
				FGameplayEffectContextHandle ContextHandle = UnitASC->MakeEffectContext();
				ContextHandle.AddInstigator(GetPawn(), GetPawn());
				
				FGameplayEffectSpecHandle SpecHandle = UnitASC->MakeOutgoingSpec(
					StrategyCardData->GameplayEffectClass, 
					1.0f, 
					ContextHandle
				);
				
				if (!SpecHandle.IsValid()) continue;
				
				if (DurationTag.IsValid())
				{
					SpecHandle.Data->SetSetByCallerMagnitude(DurationTag, StrategyCardData->Duration);
				}
				
				FActiveGameplayEffectHandle ActiveHandle = UnitASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				
				if (ActiveHandle.IsValid())
				{
					SuccessCount++;
				}
			}
			
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 成功对 %d/%d 个单位应用效果"), 
				SuccessCount, FriendlyUnits.Num());
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ GameplayEffectClass 和 EffectActorClass 都未设置！"));
			return;
		}
	}
	else
	{
		// 使用效果 Actor 模式
		UE_LOG(LogSGGameplay, Log, TEXT("  使用效果 Actor 模式"));
		
		FVector EffectLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetPawn();
		
		ASG_StrategyEffectBase* EffectActor = GetWorld()->SpawnActor<ASG_StrategyEffectBase>(
			StrategyCardData->EffectActorClass,
			EffectLocation,
			FRotator::ZeroRotator,
			SpawnParams
		);
		
		if (EffectActor)
		{
			EffectActor->InitializeEffect(StrategyCardData, GetPawn(), EffectLocation);
			EffectActor->ExecuteEffect();
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 效果 Actor 已生成并执行"));
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 效果 Actor 生成失败"));
			return;
		}
	}
    
	// 使用卡牌
	if (CardDeckComponent)
	{
		bool bSuccess = CardDeckComponent->UseCard(CardInstanceId);
		if (bSuccess)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 卡牌使用成功，进入冷却"));
		}
		else
		{
			UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 卡牌使用失败"));
		}
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_PlayerController::OnStrategyEffectFinished(ASG_StrategyEffectBase* Effect, bool bSuccess)
{
	UE_LOG(LogSGGameplay, Log, TEXT("计谋效果完成回调：%s"), bSuccess ? TEXT("成功") : TEXT("失败"));
	
	// 如果当前效果就是完成的效果，清理引用
	if (ActiveStrategyEffect == Effect)
	{
		ActiveStrategyEffect = nullptr;
		StrategyCardInstanceId.Invalidate();
		CurrentPlacementMode = ESGPlacementMode::None;
	}
}

bool ASG_PlayerController::DoesCardRequirePreview(USG_CardDataBase* CardData) const
{
	if (!CardData)
	{
		return false;
	}
    
	// 根据放置类型判断
	if (CardData->PlacementType == ESGPlacementType::Global)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  卡牌 [%s] 是全局效果，不需要预览"), 
			*CardData->CardName.ToString());
		return false;
	}
    
	// Area 和 Single 类型需要预览
	UE_LOG(LogSGGameplay, Log, TEXT("  卡牌 [%s] 需要选择目标位置"), 
		*CardData->CardName.ToString());
	return true;
}

bool ASG_PlayerController::GetMouseGroundLocation(FVector& OutLocation) const
{
	FVector WorldLocation, WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return false;
	}

	FHitResult HitResult;
	FVector TraceEnd = WorldLocation + WorldDirection * 50000.0f;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());
	
	if (GetWorld()->LineTraceSingleByChannel(HitResult, WorldLocation, TraceEnd, ECC_Visibility, QueryParams))
	{
		OutLocation = HitResult.ImpactPoint;
		return true;
	}

	return false;
}

// ========== 输入处理 ==========

void ASG_PlayerController::OnConfirmInput()
{
	UE_LOG(LogTemp, Log, TEXT("🖱️ 收到确认输入（左键点击）"));

	if (CurrentPlacementMode != ESGPlacementMode::None)
	{
		UE_LOG(LogTemp, Log, TEXT("  检测到放置模式：%d，执行确认"), static_cast<int32>(CurrentPlacementMode));
		ConfirmPlacement();
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("  无放置模式，忽略输入"));
	}
}

void ASG_PlayerController::OnCancelInput()
{
	UE_LOG(LogTemp, Log, TEXT("🖱️ 收到取消输入（右键点击）"));

	if (CurrentPlacementMode != ESGPlacementMode::None)
	{
		UE_LOG(LogTemp, Log, TEXT("  检测到放置模式：%d，执行取消"), static_cast<int32>(CurrentPlacementMode));
		CancelPlacement();
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("  无放置模式，忽略输入"));
	}
}

void ASG_PlayerController::OnCardSelectionChanged(const FGuid& SelectedId)
{
	UE_LOG(LogTemp, Log, TEXT("OnCardSelectionChanged - ID: %s"), *SelectedId.ToString());

	if (SelectedId.IsValid())
	{
		if (CardDeckComponent)
		{
			const TArray<FSGCardInstance>& Hand = CardDeckComponent->GetHand();
			
			for (const FSGCardInstance& Card : Hand)
			{
				if (Card.InstanceId == SelectedId)
				{
					UE_LOG(LogTemp, Log, TEXT("找到选中的卡牌：%s"), *Card.CardData->CardName.ToString());
					StartCardPlacement(Card.CardData, Card.InstanceId);
					return;
				}
			}

			UE_LOG(LogTemp, Warning, TEXT("⚠️ 未找到选中的卡牌"));
		}
	}
	else
	{
		// ✨ 新增 - 只有在有放置模式时才取消
		// 防止使用卡牌后触发的选中清除导致效果被取消
		if (CurrentPlacementMode != ESGPlacementMode::None)
		{
			UE_LOG(LogTemp, Log, TEXT("卡牌被取消选中，取消放置"));
			CancelPlacement();
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("卡牌被取消选中，但无放置模式，忽略"));
		}
	}
}



void ASG_PlayerController::SpawnUnitFromCard(USG_CardDataBase* CardData, const FVector& UnitSpawnLocation, const FRotator& UnitSpawnRotation)
{
	
	if (!CardData)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnUnitFromCard 失败：CardData 为空"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("========== 生成单位：%s =========="), *CardData->CardName.ToString());

	if (USG_CharacterCardData* CharacterCard = Cast<USG_CharacterCardData>(CardData))
	{
		if (!CharacterCard->CharacterClass)
		{
			UE_LOG(LogTemp, Error, TEXT("❌ 角色卡没有设置 CharacterClass"));
			return;
		}

		UE_LOG(LogSGGameplay, Log, TEXT("卡牌倍率配置："));
		UE_LOG(LogSGGameplay, Log, TEXT("  生命值倍率：%.2f"), CharacterCard->HealthMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  伤害倍率：%.2f"), CharacterCard->DamageMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  速度倍率：%.2f"), CharacterCard->SpeedMultiplier);

		if (CharacterCard->bIsTroopCard)
		{
			UE_LOG(LogTemp, Log, TEXT("生成兵团 - 阵型: %dx%d, 间距: %.0f"), 
				CharacterCard->TroopFormation.X, 
				CharacterCard->TroopFormation.Y,
				CharacterCard->TroopSpacing);

			int32 Rows = CharacterCard->TroopFormation.Y;
			int32 Cols = CharacterCard->TroopFormation.X;
			float Spacing = CharacterCard->TroopSpacing;

			FVector StartOffset = FVector(
				-(Cols - 1) * Spacing / 2.0f,
				-(Rows - 1) * Spacing / 2.0f,
				0.0f
			);

			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Cols; ++Col)
				{
					FVector UnitOffset = FVector(
						Col * Spacing,
						Row * Spacing,
						0.0f
					);

					FVector FinalUnitLocation = UnitSpawnLocation + StartOffset + UnitOffset;

					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = this;
					SpawnParams.Instigator = GetPawn();
					SpawnParams.bDeferConstruction = true;

					AActor* SpawnedUnit = GetWorld()->SpawnActor<AActor>(
						CharacterCard->CharacterClass,
						FinalUnitLocation,
						UnitSpawnRotation,
						SpawnParams
					);

					if (SpawnedUnit)
					{
						if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(SpawnedUnit))
						{
							Unit->SourceCardData = CharacterCard;
							Unit->FinishSpawning(FTransform(UnitSpawnRotation, FinalUnitLocation));
						}
						else
						{
							SpawnedUnit->FinishSpawning(FTransform(UnitSpawnRotation, FinalUnitLocation));
						}
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("✓ 兵团生成完成，共 %d 个单位"), Rows * Cols);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("生成英雄"));

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetPawn();
			SpawnParams.bDeferConstruction = true;

			AActor* SpawnedUnit = GetWorld()->SpawnActor<AActor>(
				CharacterCard->CharacterClass,
				UnitSpawnLocation,
				UnitSpawnRotation,
				SpawnParams
			);

			if (SpawnedUnit)
			{
				if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(SpawnedUnit))
				{
					Unit->SourceCardData = CharacterCard;
					Unit->FinishSpawning(FTransform(UnitSpawnRotation, UnitSpawnLocation));
				}
				else
				{
					SpawnedUnit->FinishSpawning(FTransform(UnitSpawnRotation, UnitSpawnLocation));
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

ASG_MainCityBase* ASG_PlayerController::FindEnemyMainCity()
{
	if (CachedEnemyMainCity && IsValid(CachedEnemyMainCity))
	{
		return CachedEnemyMainCity;
	}
	
	TArray<AActor*> FoundMainCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), FoundMainCities);
	
	FGameplayTag EnemyFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"));
	
	for (AActor* Actor : FoundMainCities)
	{
		ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
		if (MainCity && MainCity->FactionTag.MatchesTag(EnemyFactionTag))
		{
			CachedEnemyMainCity = MainCity;
			return CachedEnemyMainCity;
		}
	}
	
	return nullptr;
}

FRotator ASG_PlayerController::CalculateUnitSpawnRotation(const FVector& UnitLocation)
{
	ASG_MainCityBase* EnemyCity = FindEnemyMainCity();
	
	if (EnemyCity)
	{
		FVector DirectionToEnemy = EnemyCity->GetActorLocation() - UnitLocation;
		DirectionToEnemy.Z = 0.0f;
		DirectionToEnemy.Normalize();
		return DirectionToEnemy.Rotation();
	}
	
	return FRotator(0.0f, 0.0f, 0.0f);
}

