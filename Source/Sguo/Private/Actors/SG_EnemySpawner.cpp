// ✨ 新增文件 - 敌方单位生成器实现
#include "Actors/SG_EnemySpawner.h"
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Data/SG_DeckConfig.h"
// ✨ 新增头文件
#include "Buildings/SG_MainCityBase.h"
#include "Data/SG_CharacterCardData.h"
#include "Data/SG_CardDataBase.h" // 确保包含基类
#include "Units/SG_UnitsBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Debug/SG_LogCategories.h"
#include "Kismet/GameplayStatics.h"

ASG_EnemySpawner::ASG_EnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建根组件
    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootComp;

    // 创建生成区域（Box）
    SpawnAreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnAreaBox"));
    SpawnAreaBox->SetupAttachment(RootComp);
    SpawnAreaBox->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
    SpawnAreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 设置为红色以便在编辑器中区分
    SpawnAreaBox->ShapeColor = FColor::Red; 

    // 创建编辑器图标
    Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    Billboard->SetupAttachment(RootComp);
    Billboard->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    Billboard->bIsEditorOnly = true;

    // 默认阵营为敌人
    FactionTag = FGameplayTag::RequestGameplayTag(TEXT("Unit.Faction.Enemy"));
}

void ASG_EnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    // 初始化随机种子 (使用时间戳)
    RandomStream.GenerateNewSeed();
    
    // ✨ 查找关联主城
    FindRelatedMainCity();
    if (bAutoStart)
    {
        StartSpawning();
    }
}

void ASG_EnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSpawning();
    Super::EndPlay(EndPlayReason);
}

void ASG_EnemySpawner::StartSpawning()
{
    if (bIsSpawning) return;
    
    // 检查配置
    if (!DeckConfig)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("Spawner %s: DeckConfig 未设置!"), *GetName());
        return;
    }

    // 初始化池
    InitializeSpawnPool();

    if (SpawnPool.Num() == 0)
    {
        UE_LOG(LogSGGameplay, Error, TEXT("Spawner %s: 生成池为空!"), *GetName());
        return;
    }

    bIsSpawning = true;
    CurrentSpawnCount = 0;

    UE_LOG(LogSGGameplay, Log, TEXT("Spawner %s: 开始生成流程，延迟 %.2f 秒"), *GetName(), StartDelay);

    // 设置首次生成定时器
    // 如果 StartDelay <= 0，稍微延迟一帧执行，避免初始化顺序问题
    float InitialDelay = FMath::Max(StartDelay, 0.1f);
    
    GetWorld()->GetTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        &ASG_EnemySpawner::HandleSpawnTimer,
        InitialDelay,
        false
    );
}

void ASG_EnemySpawner::StopSpawning()
{
    bIsSpawning = false;
    GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
    UE_LOG(LogSGGameplay, Log, TEXT("Spawner %s: 停止生成"), *GetName());
}

void ASG_EnemySpawner::InitializeSpawnPool()
{
    SpawnPool.Empty();
    ConsumedUniqueCards.Empty();

    if (!DeckConfig) return;

    // 构建生成池（类似 SG_CardDeckComponent）
    // 这里只关注 DrawWeight 和 Pity 参数，用于随机选择单位
    for (const FSGCardConfigSlot& ConfigSlot : DeckConfig->AllowedCards)
    {
        // 加载卡牌数据 (同步加载，因为此时通常已经是运行时)
        // 注意：为了更安全，建议确保 DeckConfig 中的资源已被 Manager 预加载
        // 这里使用 LoadSynchronous 简化流程
        USG_CardDataBase* CardAsset = ConfigSlot.CardData.LoadSynchronous();
        
        if (!CardAsset) continue;

        FSGCardDrawSlot Slot;
        Slot.CardId = CardAsset->GetPrimaryAssetId();
        Slot.DrawWeight = FMath::Max(0.0f, ConfigSlot.DrawWeight);
        Slot.PityMultiplier = FMath::Max(0.0f, ConfigSlot.PityMultiplier);
        Slot.PityMaxMultiplier = FMath::Max(1.0f, ConfigSlot.PityMaxMultiplier);
        // 对于刷怪器，MaxOccurrences 通常指这一波或总共能刷多少个
        // 这里我们使用 ConfigSlot.MaxOccurrences 作为单卡限制
        Slot.MaxOccurrences = ConfigSlot.MaxOccurrences; 
        
        SpawnPool.Add(Slot);
    }
}

void ASG_EnemySpawner::HandleSpawnTimer()
{
    if (!bIsSpawning) return;
    // ✨ 检查主城是否存活
    if (RelatedMainCity.IsValid())
    {
        if (!RelatedMainCity->IsAlive())
        {
            UE_LOG(LogSGGameplay, Log, TEXT("Spawner %s: 主城已摧毁，停止生成"), *GetName());
            StopSpawning();
            return;
        }
    }
    
    // 执行生成
    bool bSpawnSuccess = SpawnNextWave();

    // 检查总数量限制
    if (MaxSpawnCount > 0 && CurrentSpawnCount >= MaxSpawnCount)
    {
        UE_LOG(LogSGGameplay, Log, TEXT("Spawner %s: 达到最大生成数量 %d，停止"), *GetName(), MaxSpawnCount);
        StopSpawning();
        return;
    }

    // 如果生成失败（比如卡池空了），尝试停止或等待
    // 这里选择继续尝试，也许是等待唯一卡重置（虽然唯一卡一般不重置）
    // 如果 SpawnPool 为空，强制停止
    if (SpawnPool.Num() == 0)
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("Spawner %s: 卡池耗尽，停止"), *GetName());
        StopSpawning();
        return;
    }

    // 计算下一次间隔
    float NextInterval = GetNextSpawnInterval();
    
    UE_LOG(LogSGGameplay, Verbose, TEXT("Spawner %s: 下一次生成在 %.2f 秒后"), *GetName(), NextInterval);

    GetWorld()->GetTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        &ASG_EnemySpawner::HandleSpawnTimer,
        NextInterval,
        false
    );
}

bool ASG_EnemySpawner::SpawnNextWave()
{
    USG_CardDataBase* SelectedCard = DrawCardFromPool();
    
    if (!SelectedCard)
    {
        return false;
    }

    // 确定生成位置中心点
    FVector SpawnLocation;
    if (LocationMode == ESGSpawnLocationMode::CenterOfArea)
    {
        SpawnLocation = SpawnAreaBox->GetComponentLocation();
    }
    else
    {
        SpawnLocation = GetRandomSpawnLocation();
    }

    // 生成单位（处理兵团逻辑）
    SpawnUnit(SelectedCard, SpawnLocation);

    return true;
}

USG_CardDataBase* ASG_EnemySpawner::DrawCardFromPool()
{
    // 1. 过滤有效槽位
    TArray<FSGCardDrawSlot*> ValidSlots;
    float TotalWeight = 0.0f;

    for (FSGCardDrawSlot& Slot : SpawnPool)
    {
        // 检查唯一卡是否已消耗
        if (ConsumedUniqueCards.Contains(Slot.CardId)) continue;
        
        // 检查单卡最大生成次数
        if (Slot.MaxOccurrences > 0 && Slot.OccurrenceCount >= Slot.MaxOccurrences) continue;

        // 检查权重
        if (Slot.DrawWeight <= 0.0f) continue;

        ValidSlots.Add(&Slot);
        TotalWeight += Slot.GetEffectiveWeight();
    }

    if (ValidSlots.Num() == 0) return nullptr;

    // 2. 轮盘赌选择
    float RandomValue = RandomStream.FRandRange(0.0f, TotalWeight);
    float CurrentWeight = 0.0f;
    FSGCardDrawSlot* SelectedSlot = nullptr;

    for (FSGCardDrawSlot* Slot : ValidSlots)
    {
        CurrentWeight += Slot->GetEffectiveWeight();
        if (RandomValue <= CurrentWeight)
        {
            SelectedSlot = Slot;
            break;
        }
    }

    if (!SelectedSlot) SelectedSlot = ValidSlots.Last();

    // 3. 更新保底和计数
    for (FSGCardDrawSlot* Slot : ValidSlots)
    {
        if (Slot == SelectedSlot)
        {
            Slot->MissCount = 0;
            Slot->OccurrenceCount++;
        }
        else
        {
            Slot->MissCount++;
        }
    }

    // 4. 获取资源 (修复部分)
    // 我们需要从 DeckConfig 中找到对应的 SoftPtr 并加载
    for (const FSGCardConfigSlot& ConfigSlot : DeckConfig->AllowedCards)
    {
        // 🔧 修改 - 使用资产名称进行匹配
        // TSoftObjectPtr::GetAssetName() 返回资产名称字符串 (FString)
        // FPrimaryAssetId::PrimaryAssetName 是 FName
        if (FName(*ConfigSlot.CardData.GetAssetName()) == SelectedSlot->CardId.PrimaryAssetName)
        {
            // 同步加载卡牌数据
            USG_CardDataBase* Card = ConfigSlot.CardData.LoadSynchronous();
             
            // 处理唯一卡逻辑
            if (Card && Card->bIsUnique)
            {
                ConsumedUniqueCards.Add(SelectedSlot->CardId);
            }
            return Card;
        }
    }

    return nullptr;
}

float ASG_EnemySpawner::GetNextSpawnInterval() const
{
    switch (IntervalMethod)
    {
    case ESGSpawnIntervalMethod::UseDeckCooldown:
        return DeckConfig ? DeckConfig->DrawCDSeconds : 2.0f;
        
    case ESGSpawnIntervalMethod::FixedInterval:
        return FixedSpawnInterval;
        
    case ESGSpawnIntervalMethod::RandomInterval:
        return FMath::RandRange(MinSpawnInterval, MaxSpawnInterval);
        
    default:
        return 2.0f;
    }
}

void ASG_EnemySpawner::SpawnUnit(USG_CardDataBase* CardData, const FVector& CenterLocation)
{
  USG_CharacterCardData* CharCard = Cast<USG_CharacterCardData>(CardData);
    if (!CharCard || !CharCard->CharacterClass) return;

    // 🔧 关键修改：获取胶囊体半高
    float CapsuleHalfHeight = 88.0f;
    ACharacter* CharCDO = Cast<ACharacter>(CharCard->CharacterClass->GetDefaultObject());
    if (CharCDO && CharCDO->GetCapsuleComponent())
    {
        CapsuleHalfHeight = CharCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    float SpawnZOffset = CapsuleHalfHeight + 2.0f;

    // 检查是否是兵团
    if (CharCard->bIsTroopCard)
    {
        int32 Rows = CharCard->TroopFormation.Y;
        int32 Cols = CharCard->TroopFormation.X;
        float Spacing = CharCard->TroopSpacing;

        FVector StartOffset = FVector(
            -(Cols - 1) * Spacing / 2.0f,
            -(Rows - 1) * Spacing / 2.0f,
            0.0f
        );

        for (int32 Row = 0; Row < Rows; ++Row)
        {
            for (int32 Col = 0; Col < Cols; ++Col)
            {
                FVector UnitOffset = FVector(Col * Spacing, Row * Spacing, 0.0f);
                FVector RotatedOffset = SpawnRotation.RotateVector(StartOffset + UnitOffset);
                FVector FinalLoc = CenterLocation + RotatedOffset;

                // 地面吸附逻辑
                FHitResult HitResult;
                FVector TraceStart = FinalLoc + FVector(0, 0, 500.0f);
                FVector TraceEnd = FinalLoc - FVector(0, 0, 1000.0f);
                FCollisionQueryParams QueryParams;
                QueryParams.AddIgnoredActor(this);

                if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
                {
                    // 🔧 使用动态计算的高度
                    FinalLoc = HitResult.Location + FVector(0.0f, 0.0f, SpawnZOffset);
                }

                FTransform SpawnTransform(SpawnRotation, FinalLoc);
                ASG_UnitsBase* NewUnit = GetWorld()->SpawnActorDeferred<ASG_UnitsBase>(
                    CharCard->CharacterClass,
                    SpawnTransform,
                    this,
                    nullptr,
                    ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
                );

                if (NewUnit)
                {
                    NewUnit->SetSourceCardData(CharCard);
                    NewUnit->FactionTag = FactionTag;
                    NewUnit->FinishSpawning(SpawnTransform);
                    CurrentSpawnCount++;
                }
            }
        }
    }
    else
    {
        // 生成单个英雄
        FVector FinalLoc = CenterLocation;

        FHitResult HitResult;
        FVector TraceStart = FinalLoc + FVector(0, 0, 500.0f);
        FVector TraceEnd = FinalLoc - FVector(0, 0, 1000.0f);
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
        {
            // 🔧 使用动态计算的高度
            FinalLoc = HitResult.Location + FVector(0.0f, 0.0f, SpawnZOffset);
        }

        FTransform SpawnTransform(SpawnRotation, FinalLoc);
        ASG_UnitsBase* NewUnit = GetWorld()->SpawnActorDeferred<ASG_UnitsBase>(
            CharCard->CharacterClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
        );

        if (NewUnit)
        {
            NewUnit->SetSourceCardData(CharCard);
            NewUnit->FactionTag = FactionTag;
            NewUnit->FinishSpawning(SpawnTransform);
            CurrentSpawnCount++;
        }
    }
}

FVector ASG_EnemySpawner::GetRandomSpawnLocation() const
{
    FVector Origin = SpawnAreaBox->GetComponentLocation();
    FVector BoxExtent = SpawnAreaBox->GetScaledBoxExtent();
    return UKismetMathLibrary::RandomPointInBoundingBox(Origin, BoxExtent);
}

void ASG_EnemySpawner::FindRelatedMainCity()
{
    TArray<AActor*> AllMainCities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASG_MainCityBase::StaticClass(), AllMainCities);

    for (AActor* Actor : AllMainCities)
    {
        ASG_MainCityBase* City = Cast<ASG_MainCityBase>(Actor);
        if (City && City->FactionTag.MatchesTag(FactionTag))
        {
            RelatedMainCity = City;
            UE_LOG(LogSGGameplay, Log, TEXT("Spawner %s: 已关联主城 %s"), *GetName(), *City->GetName());
            break;
        }
    }

    if (!RelatedMainCity.IsValid())
    {
        UE_LOG(LogSGGameplay, Warning, TEXT("Spawner %s: 未找到同阵营(%s)的主城，无法检测主城存活状态"), 
            *GetName(), *FactionTag.ToString());
    }
}
