// 🔧 MODIFIED FILE - 玩家控制器实现
// Copyright notice placeholder
/**
 * @file SG_PlayerController.cpp
 * @brief 玩家控制器实现
 */
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
#include "Strategies/SG_StrategyEffectBase.h"

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
	
	// ✨ NEW - 尝试立即绑定 Pawn 事件（如果 Pawn 已就绪）
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

void ASG_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Log, TEXT("SetupInputComponent 被调用"));

}

// ✨ NEW - 在 Pawn 被占有时绑定事件
void ASG_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	UE_LOG(LogTemp, Log, TEXT("========== OnPossess 被调用 =========="));
	UE_LOG(LogTemp, Log, TEXT("Pawn: %s"), InPawn ? *InPawn->GetName() : TEXT("nullptr"));
	
	// 绑定 Pawn 输入事件
	BindPawnInputEvents();
	
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

// ✨ NEW - 绑定 Pawn 输入事件
void ASG_PlayerController::BindPawnInputEvents()
{
	// 防止重复绑定
	if (bPawnInputBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pawn 输入事件已绑定，跳过"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("========== 绑定 Pawn 输入事件 =========="));
	
	// 获取 Pawn
	ASG_Player* PlayerPawn = Cast<ASG_Player>(GetPawn());
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 未找到 PlayerPawn！"));
		UE_LOG(LogTemp, Error, TEXT("   当前 Pawn: %s"), GetPawn() ? *GetPawn()->GetName() : TEXT("nullptr"));
		UE_LOG(LogTemp, Error, TEXT("   请检查："));
		UE_LOG(LogTemp, Error, TEXT("   1. GameMode 中是否设置了 Default Pawn Class 为 BP_SGPlayer"));
		UE_LOG(LogTemp, Error, TEXT("   2. BP_SGPlayer 是否继承自 ASG_Player"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("找到 PlayerPawn: %s"), *PlayerPawn->GetName());
	
	// 绑定确认输入（左键）
	PlayerPawn->OnConfirmInput.AddDynamic(this, &ASG_PlayerController::OnConfirmInput);
	UE_LOG(LogTemp, Log, TEXT("  ✓ 已绑定确认输入（左键）"));
	
	// 绑定取消输入（右键）
	PlayerPawn->OnCancelInput.AddDynamic(this, &ASG_PlayerController::OnCancelInput);
	UE_LOG(LogTemp, Log, TEXT("  ✓ 已绑定取消输入（右键）"));
	
	// 标记已绑定
	bPawnInputBound = true;
	
	UE_LOG(LogTemp, Log, TEXT("✓ Pawn 输入事件绑定完成"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

USG_CardDeckComponent* ASG_PlayerController::GetCardDeckComponent() const
{
	return CardDeckComponent;
}

void ASG_PlayerController::StartCardPlacement(USG_CardDataBase* CardData, const FGuid& CardInstanceId)
{
	if (!CardData)
	{
		UE_LOG(LogTemp, Error, TEXT("StartCardPlacement 失败：CardData 为空"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("========== 开始放置卡牌：%s =========="), *CardData->CardName.ToString());

	// ✨ 新增 - 检查是否需要预览
	if (!DoesCardRequirePreview(CardData))
	{
		// 全局效果计谋卡，直接使用
		USG_StrategyCardData* StrategyCard = Cast<USG_StrategyCardData>(CardData);
		if (StrategyCard)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  全局效果卡牌，直接使用"));
			UseStrategyCardDirectly(StrategyCard, CardInstanceId);
            
			// 清除选中状态
			if (CardDeckComponent)
			{
				CardDeckComponent->SelectCard(FGuid());
			}
			return;
		}
	}

	// 需要预览的卡牌，继续原有逻辑
	if (!PlacementPreviewClass)
	{
		UE_LOG(LogTemp, Error, TEXT("StartCardPlacement 失败：PlacementPreviewClass 未设置"));
		return;
	}

	if (CurrentPreviewActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("已存在预览 Actor，先销毁"));
		CurrentPreviewActor->Destroy();
		CurrentPreviewActor = nullptr;
	}

	CurrentSelectedCardData = CardData;
	CurrentSelectedCardInstanceId = CardInstanceId;

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
	}
}

void ASG_PlayerController::ConfirmPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("========== 确认放置卡牌 =========="));

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

	// 获取生成位置
	FVector UnitSpawnLocation = CurrentPreviewActor->GetPreviewLocation();
	FRotator UnitSpawnRotation = CalculateUnitSpawnRotation(UnitSpawnLocation);

	// 输出生成信息
	UE_LOG(LogSGGameplay, Log, TEXT("放置位置：%s"), *UnitSpawnLocation.ToString());
	UE_LOG(LogSGGameplay, Log, TEXT("放置旋转：%s"), *UnitSpawnRotation.ToString());

	// ✨ 新增 - 检查是否是计谋卡
	USG_StrategyCardData* StrategyCard = Cast<USG_StrategyCardData>(CurrentSelectedCardData);
	if (StrategyCard)
	{
		// 区域效果计谋卡
		UE_LOG(LogSGGameplay, Log, TEXT("使用区域计谋卡：%s"), *StrategyCard->CardName.ToString());
		UseStrategyCard(StrategyCard, UnitSpawnLocation);
	}
	else
	{
		// 角色卡
		SpawnUnitFromCard(CurrentSelectedCardData, UnitSpawnLocation, UnitSpawnRotation);
	}

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

	// 销毁预览 Actor
	if (CurrentPreviewActor)
	{
		CurrentPreviewActor->Destroy();
		CurrentPreviewActor = nullptr;
	}

	CurrentSelectedCardData = nullptr;
	CurrentSelectedCardInstanceId.Invalidate();

	UE_LOG(LogTemp, Log, TEXT("✓ 放置完成"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void ASG_PlayerController::CancelPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("========== 取消放置卡牌 =========="));

	if (CurrentPreviewActor)
	{
		CurrentPreviewActor->Destroy();
		CurrentPreviewActor = nullptr;
		UE_LOG(LogTemp, Log, TEXT("✓ 预览 Actor 已销毁"));
	}

	if (CardDeckComponent && CurrentSelectedCardInstanceId.IsValid())
	{
		CardDeckComponent->SelectCard(FGuid());
		UE_LOG(LogTemp, Log, TEXT("✓ 已取消选中卡牌"));
	}

	CurrentSelectedCardData = nullptr;
	CurrentSelectedCardInstanceId.Invalidate();

	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void ASG_PlayerController::UseStrategyCard(USG_StrategyCardData* StrategyCardData, const FVector& TargetLocation)
{
	// 检查参数有效性
	if (!StrategyCardData)
	{
		UE_LOG(LogTemp, Error, TEXT("UseStrategyCard 失败：StrategyCardData 为空"));
		return;
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("========== 使用计谋卡：%s =========="), 
		*StrategyCardData->CardName.ToString());
    
	// 检查效果 Actor 类是否设置
	if (!StrategyCardData->EffectActorClass)
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ EffectActorClass 未设置！"));
		return;
	}
    
	// 生成效果 Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetPawn();
    
	ASG_StrategyEffectBase* EffectActor = GetWorld()->SpawnActor<ASG_StrategyEffectBase>(
		StrategyCardData->EffectActorClass,
		TargetLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);
    
	if (EffectActor)
	{
		// 初始化效果
		EffectActor->InitializeEffect(StrategyCardData, GetPawn(), TargetLocation);
        
		// 执行效果
		EffectActor->ExecuteEffect();
        
		UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 计谋效果已生成并执行"));
	}
	else
	{
		UE_LOG(LogSGGameplay, Error, TEXT("  ❌ 效果 Actor 生成失败"));
	}
    
	UE_LOG(LogSGGameplay, Log, TEXT("========================================"));
}

void ASG_PlayerController::OnConfirmInput()
{
	UE_LOG(LogTemp, Log, TEXT("🖱️ 收到确认输入（左键点击）"));

	if (CurrentPreviewActor)
	{
		UE_LOG(LogTemp, Log, TEXT("  检测到预览 Actor，执行确认放置"));
		ConfirmPlacement();
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("  无预览 Actor，忽略输入"));
	}
}

void ASG_PlayerController::OnCancelInput()
{
	UE_LOG(LogTemp, Log, TEXT("🖱️ 收到取消输入（右键点击）"));

	if (CurrentPreviewActor)
	{
		UE_LOG(LogTemp, Log, TEXT("  检测到预览 Actor，执行取消放置"));
		CancelPlacement();
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("  无预览 Actor，忽略输入"));
	}
}

/**
 * @brief 根据卡牌数据生成单位
 * @param CardData 卡牌数据
 * @param UnitSpawnLocation 单位生成位置
 * @param UnitSpawnRotation 单位生成旋转
 * @details
 * 功能说明：
 * - 根据卡牌类型生成单位或计谋效果
 * - 🔧 修改 - 使用 SpawnActorDeferred 在 BeginPlay 前设置 SourceCardData
 * 详细流程：
 * 1. 检查卡牌类型（角色卡/计谋卡）
 * 2. 使用 SpawnActorDeferred 延迟生成单位
 * 3. 设置 SourceCardData 引用
 * 4. 调用 FinishSpawning 触发 BeginPlay
 * 5. BeginPlay 中自动读取倍率并初始化
 * 注意事项：
 * - 必须使用 SpawnActorDeferred + FinishSpawning
 * - 否则 BeginPlay 会在设置 SourceCardData 之前执行
 */
void ASG_PlayerController::SpawnUnitFromCard(USG_CardDataBase* CardData, const FVector& UnitSpawnLocation, const FRotator& UnitSpawnRotation)
{
	// ========== 步骤1：检查卡牌数据有效性 ==========
	if (!CardData)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnUnitFromCard 失败：CardData 为空"));
		return;
	}

	// 输出日志
	UE_LOG(LogTemp, Log, TEXT("========== 生成单位：%s =========="), *CardData->CardName.ToString());

	// ========== 步骤2：处理角色卡 ==========
	if (USG_CharacterCardData* CharacterCard = Cast<USG_CharacterCardData>(CardData))
	{
		// 检查角色类是否设置
		if (!CharacterCard->CharacterClass)
		{
			UE_LOG(LogTemp, Error, TEXT("❌ 角色卡没有设置 CharacterClass"));
			return;
		}

		// ✨ 新增 - 输出卡牌倍率信息
		UE_LOG(LogSGGameplay, Log, TEXT("卡牌倍率配置："));
		UE_LOG(LogSGGameplay, Log, TEXT("  生命值倍率：%.2f"), CharacterCard->HealthMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  伤害倍率：%.2f"), CharacterCard->DamageMultiplier);
		UE_LOG(LogSGGameplay, Log, TEXT("  速度倍率：%.2f"), CharacterCard->SpeedMultiplier);

		// ========== 分支1：生成兵团 ==========
		if (CharacterCard->bIsTroopCard)
		{
			// 输出日志
			UE_LOG(LogTemp, Log, TEXT("生成兵团 - 阵型: %dx%d, 间距: %.0f"), 
				CharacterCard->TroopFormation.X, 
				CharacterCard->TroopFormation.Y,
				CharacterCard->TroopSpacing);

			// 获取阵型参数
			int32 Rows = CharacterCard->TroopFormation.Y;
			int32 Cols = CharacterCard->TroopFormation.X;
			float Spacing = CharacterCard->TroopSpacing;

			// 计算起始偏移（使阵型中心对齐）
			FVector StartOffset = FVector(
				-(Cols - 1) * Spacing / 2.0f,
				-(Rows - 1) * Spacing / 2.0f,
				0.0f
			);

			// 遍历阵型位置
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Cols; ++Col)
				{
					// 计算单位偏移
					FVector UnitOffset = FVector(
						Col * Spacing,
						Row * Spacing,
						0.0f
					);

					// 计算最终位置
					FVector FinalUnitLocation = UnitSpawnLocation + StartOffset + UnitOffset;

					// 🔧 修改 - 使用延迟生成模式
					// 设置生成参数
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = this;
					SpawnParams.Instigator = GetPawn();
					// ✨ 关键 - 延迟生成，不立即调用 BeginPlay
					SpawnParams.bDeferConstruction = true;

					// 延迟生成单位 Actor（不会触发 BeginPlay）
					AActor* SpawnedUnit = GetWorld()->SpawnActor<AActor>(
						CharacterCard->CharacterClass,
						FinalUnitLocation,
						UnitSpawnRotation,
						SpawnParams
					);

					// 检查生成是否成功
					if (SpawnedUnit)
					{
						// 输出日志
						UE_LOG(LogTemp, Verbose, TEXT("  ✓ 延迟生成单位 [%d,%d] 于 %s"), Row, Col, *FinalUnitLocation.ToString());

						// 转换为 ASG_UnitsBase
						if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(SpawnedUnit))
						{
							// ✨ 关键步骤 - 在 BeginPlay 之前设置 SourceCardData
							Unit->SourceCardData = CharacterCard;
							UE_LOG(LogSGGameplay, Verbose, TEXT("    已设置 SourceCardData（BeginPlay 前）"));
							
							// ✨ 关键步骤 - 完成生成，触发 BeginPlay
							// 此时 SourceCardData 已经设置，BeginPlay 可以正确读取
							Unit->FinishSpawning(FTransform(UnitSpawnRotation, FinalUnitLocation));
							UE_LOG(LogSGGameplay, Verbose, TEXT("    已完成生成，BeginPlay 已触发"));
						}
						else
						{
							// 如果转换失败，也需要完成生成
							UE_LOG(LogTemp, Warning, TEXT("  ⚠️ 单位不是 ASG_UnitsBase 类型，仍完成生成"));
							SpawnedUnit->FinishSpawning(FTransform(UnitSpawnRotation, FinalUnitLocation));
						}
					}
					else
					{
						// 输出错误日志
						UE_LOG(LogTemp, Error, TEXT("  ❌ 单位生成失败 [%d,%d]"), Row, Col);
					}
				}
			}

			// 输出日志
			UE_LOG(LogTemp, Log, TEXT("✓ 兵团生成完成，共 %d 个单位"), Rows * Cols);
			UE_LOG(LogSGGameplay, Log, TEXT("  所有单位已在 BeginPlay 前设置 SourceCardData"));
		}
		// ========== 分支2：生成英雄 ==========
		else
		{
			// 输出日志
			UE_LOG(LogTemp, Log, TEXT("生成英雄"));

			// 🔧 修改 - 使用延迟生成模式
			// 设置生成参数
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetPawn();
			// ✨ 关键 - 延迟生成，不立即调用 BeginPlay
			SpawnParams.bDeferConstruction = true;

			// 延迟生成单位 Actor（不会触发 BeginPlay）
			AActor* SpawnedUnit = GetWorld()->SpawnActor<AActor>(
				CharacterCard->CharacterClass,
				UnitSpawnLocation,
				UnitSpawnRotation,
				SpawnParams
			);

			// 检查生成是否成功
			if (SpawnedUnit)
			{
				// 输出日志
				UE_LOG(LogTemp, Log, TEXT("✓ 英雄延迟生成成功"));

				// 转换为 ASG_UnitsBase
				if (ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(SpawnedUnit))
				{
					// ✨ 关键步骤 - 在 BeginPlay 之前设置 SourceCardData
					Unit->SourceCardData = CharacterCard;
					UE_LOG(LogSGGameplay, Log, TEXT("  已设置 SourceCardData（BeginPlay 前）"));
					
					// ✨ 关键步骤 - 完成生成，触发 BeginPlay
					// 此时 SourceCardData 已经设置，BeginPlay 可以正确读取
					Unit->FinishSpawning(FTransform(UnitSpawnRotation, UnitSpawnLocation));
					UE_LOG(LogSGGameplay, Log, TEXT("  已完成生成，BeginPlay 已触发"));
				}
				else
				{
					// 如果转换失败，也需要完成生成
					UE_LOG(LogTemp, Warning, TEXT("  ⚠️ 单位不是 ASG_UnitsBase 类型，仍完成生成"));
					SpawnedUnit->FinishSpawning(FTransform(UnitSpawnRotation, UnitSpawnLocation));
				}
			}
			else
			{
				// 输出错误日志
				UE_LOG(LogTemp, Error, TEXT("❌ 英雄生成失败"));
			}
		}
	}
	// ========== 步骤3：处理计谋卡 ==========
	else if (USG_StrategyCardData* StrategyCard = Cast<USG_StrategyCardData>(CardData))
	{
		// 输出日志
		UE_LOG(LogTemp, Log, TEXT("生成计谋效果"));
    
		// ✨ 新增 - 调用 UseStrategyCard
		UseStrategyCard(StrategyCard, UnitSpawnLocation);
	}
	// ========== 步骤4：未知卡牌类型 ==========
	else
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("❌ 未知的卡牌类型"));
	}

	// 输出日志
	UE_LOG(LogTemp, Log, TEXT("========================================"));
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
		if (CurrentPreviewActor)
		{
			UE_LOG(LogTemp, Log, TEXT("卡牌被取消选中，取消放置"));
			CancelPlacement();
		}
	}

}


/**
 * @brief 查找敌方主城
 * @return 敌方主城 Actor
 * @details
 * 功能说明：
 * - 在场景中查找敌方阵营的主城
 * - 结果会被缓存，避免重复查找
 * 详细流程：
 * 1. 检查缓存是否有效
 * 2. 获取场景中所有主城
 * 3. 筛选敌方阵营的主城
 * 4. 缓存并返回结果
 */
ASG_MainCityBase* ASG_PlayerController::FindEnemyMainCity()
{
    // 如果已经缓存，直接返回
    if (CachedEnemyMainCity && IsValid(CachedEnemyMainCity))
    {
        return CachedEnemyMainCity;
    }
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("查找敌方主城..."));
    
    // 获取场景中所有主城
    TArray<AActor*> FoundMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), FoundMainCities);
    
    // 输出日志
    UE_LOG(LogSGGameplay, Log, TEXT("  找到 %d 个主城"), FoundMainCities.Num());
    
    // 敌方阵营标签
    FGameplayTag EnemyFactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"));
    
    // 遍历所有主城
    for (AActor* Actor : FoundMainCities)
    {
        // 转换为主城类型
        ASG_MainCityBase* MainCity = Cast<ASG_MainCityBase>(Actor);
        // 如果转换失败，跳过
        if (!MainCity)
        {
            continue;
        }
        
        // 检查阵营标签是否匹配
        if (MainCity->FactionTag.MatchesTag(EnemyFactionTag))
        {
            // 找到敌方主城，缓存起来
            CachedEnemyMainCity = MainCity;
            // 输出日志
            UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 找到敌方主城：%s（位置：%s）"), 
                *MainCity->GetName(), 
                *MainCity->GetActorLocation().ToString());
            // 返回敌方主城
            return CachedEnemyMainCity;
        }
    }
    
    // 未找到敌方主城
    UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 未找到敌方主城"));
    // 返回 nullptr
    return nullptr;
}


/**
 * @brief 计算单位生成朝向
 * @param UnitLocation 单位生成位置
 * @return 朝向旋转
 * @details
 * 功能说明：
 * - 根据敌方主城位置动态计算朝向
 * - 如果未找到敌方主城，使用默认朝向（+X 方向）
 * 详细流程：
 * 1. 查找敌方主城
 * 2. 如果找到，计算从生成位置到主城的方向
 * 3. 将方向转换为旋转
 * 4. 如果未找到，返回默认朝向
 * 注意事项：
 * - 只考虑水平方向（Z = 0）
 * - 参数名改为 UnitLocation，避免与 APlayerController::SpawnLocation 冲突
 */
FRotator ASG_PlayerController::CalculateUnitSpawnRotation(const FVector& UnitLocation)
{
    // 查找敌方主城
    ASG_MainCityBase* EnemyCity = FindEnemyMainCity();
    
    // 如果找到敌方主城
    if (EnemyCity)
    {
        // 计算从生成位置到敌方主城的方向向量
        FVector DirectionToEnemy = EnemyCity->GetActorLocation() - UnitLocation;
        // 只考虑水平方向，忽略高度差
        DirectionToEnemy.Z = 0.0f;
        // 归一化方向向量
        DirectionToEnemy.Normalize();
        
        // 将方向向量转换为旋转
        FRotator Rotation = DirectionToEnemy.Rotation();
        
        // 输出日志
        UE_LOG(LogSGGameplay, Verbose, TEXT("计算朝向：%s（朝向敌方主城）"), *Rotation.ToString());
        
        // 返回旋转
        return Rotation;
    }
    
    // 未找到敌方主城，使用默认朝向（+X 方向，即 Yaw = 0）
    UE_LOG(LogSGGameplay, Verbose, TEXT("使用默认朝向：+X 方向"));
    // 返回默认旋转
    return FRotator(0.0f, 0.0f, 0.0f);
}

bool ASG_PlayerController::DoesCardRequirePreview(USG_CardDataBase* CardData) const
{
	if (!CardData)
	{
		return false;
	}
    
	// 检查是否是计谋卡
	USG_StrategyCardData* StrategyCard = Cast<USG_StrategyCardData>(CardData);
	if (StrategyCard)
	{
		// 根据放置类型判断
		// Global（全局）：不需要预览
		// Single（单点）或 Area（区域）：需要预览
		if (CardData->PlacementType == ESGPlacementType::Global)
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  计谋卡 [%s] 是全局效果，不需要预览"), 
				*CardData->CardName.ToString());
			return false;
		}
		else
		{
			UE_LOG(LogSGGameplay, Log, TEXT("  计谋卡 [%s] 需要选择目标位置"), 
				*CardData->CardName.ToString());
			return true;
		}
	}
    
	// 角色卡：需要预览
	USG_CharacterCardData* CharacterCard = Cast<USG_CharacterCardData>(CardData);
	if (CharacterCard)
	{
		UE_LOG(LogSGGameplay, Log, TEXT("  角色卡 [%s] 需要预览"), 
			*CardData->CardName.ToString());
		return true;
	}
    
	// 默认需要预览
	return true;
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
    
    // ========== 分支1：纯 GE 效果（如神速计、强攻计）==========
    if (StrategyCardData->GameplayEffectClass && !StrategyCardData->EffectActorClass)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  使用纯 GE 模式"));
        UE_LOG(LogSGGameplay, Log, TEXT("  持续时间：%.1f 秒"), StrategyCardData->Duration);
        
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
        
        // ✨ 新增 - 获取 Duration Tag（用于 SetByCaller）
        FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"), false);
        
        // 对每个友方单位应用 GE
        int32 SuccessCount = 0;
        for (AActor* Actor : FriendlyUnits)
        {
            ASG_UnitsBase* Unit = Cast<ASG_UnitsBase>(Actor);
            if (!Unit)
            {
                continue;
            }
            
            UAbilitySystemComponent* UnitASC = Unit->GetAbilitySystemComponent();
            if (!UnitASC)
            {
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 单位 %s 没有 ASC"), *Unit->GetName());
                continue;
            }
            
            // 创建效果上下文
            FGameplayEffectContextHandle ContextHandle = UnitASC->MakeEffectContext();
            ContextHandle.AddInstigator(GetPawn(), GetPawn());
            
            // 创建效果规格
            FGameplayEffectSpecHandle SpecHandle = UnitASC->MakeOutgoingSpec(
                StrategyCardData->GameplayEffectClass, 
                1.0f, 
                ContextHandle
            );
            
            if (!SpecHandle.IsValid())
            {
                UE_LOG(LogSGGameplay, Warning, TEXT("  ⚠️ 无法为 %s 创建 GE 规格"), *Unit->GetName());
                continue;
            }
            
            // ✨ 新增 - 通过 SetByCaller 传递 Duration
            if (DurationTag.IsValid())
            {
                SpecHandle.Data->SetSetByCallerMagnitude(DurationTag, StrategyCardData->Duration);
                UE_LOG(LogSGGameplay, Verbose, TEXT("  设置 Duration = %.1f"), StrategyCardData->Duration);
            }
            
            // 应用效果
            FActiveGameplayEffectHandle ActiveHandle = UnitASC->ApplyGameplayEffectSpecToSelf(
                *SpecHandle.Data.Get()
            );
            
            if (ActiveHandle.IsValid())
            {
                SuccessCount++;
                UE_LOG(LogSGGameplay, Verbose, TEXT("  ✓ 对 %s 应用效果成功"), *Unit->GetName());
            }
        }
        
        UE_LOG(LogSGGameplay, Log, TEXT("  ✓ 成功对 %d/%d 个单位应用效果"), 
            SuccessCount, FriendlyUnits.Num());
    }
    // ========== 分支2：效果 Actor 模式（如流木计、火矢计）==========
    else if (StrategyCardData->EffectActorClass)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("  使用效果 Actor 模式"));
        
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = GetPawn();
        
        FVector EffectLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
        
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
    // ========== 分支3：都没设置 ==========
    else
    {
        UE_LOG(LogSGGameplay, Error, TEXT("  ❌ GameplayEffectClass 和 EffectActorClass 都未设置！"));
        return;
    }
    
    // ========== 使用卡牌（进入冷却）==========
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
