// 📄 文件：Source/Sguo/Public/Player/SG_PlayerController.h
// 🔧 修改 - 低耦合计谋卡处理

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SG_PlayerController.generated.h"

class ASG_MainCityBase;
class UInputMappingContext;
class USG_CardDeckComponent;
class USG_CardHandWidget;
class ASG_PlacementPreview;
class USG_CardDataBase;
class USG_StrategyCardData;
// ✨ 新增 - 计谋效果基类前向声明
class ASG_StrategyEffectBase;

// ✨ 新增 - 放置模式枚举
/**
 * @brief 放置模式枚举
 * @details 定义当前的卡牌放置/选择模式
 */
UENUM(BlueprintType)
enum class ESGPlacementMode : uint8
{
	// 无放置模式
	None                UMETA(DisplayName = "无"),
	
	// 普通卡牌放置（单位/英雄）
	CardPlacement       UMETA(DisplayName = "卡牌放置"),
	
	// 计谋卡目标选择（通用，不区分具体类型）
	StrategyTarget      UMETA(DisplayName = "计谋目标选择")
};

UCLASS()
class SGUO_API ASG_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASG_PlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Enhanced", meta = (AllowPrivateAccess = "true"))
	int32 MappingContextPriority = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardDeckComponent> CardDeckComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<USG_CardHandWidget> CardHandWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardHandWidget> CardHandWidget;

	// ========== 放置系统 ==========
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ASG_PlacementPreview> PlacementPreviewClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ASG_PlacementPreview> CurrentPreviewActor;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardDataBase> CurrentSelectedCardData;

	FGuid CurrentSelectedCardInstanceId;
	
	bool bPawnInputBound = false;

	// ✨ 新增 - 当前放置模式
	UPROPERTY(BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true", DisplayName = "当前放置模式"))
	ESGPlacementMode CurrentPlacementMode = ESGPlacementMode::None;

	// ========== ✨ 新增 - 计谋卡相关（通用，低耦合）==========
	
	/**
	 * @brief 当前活跃的计谋效果
	 * @details 用于跟踪正在选择目标的计谋效果（任何类型）
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Strategy", 
		meta = (AllowPrivateAccess = "true", DisplayName = "当前计谋效果"))
	TObjectPtr<ASG_StrategyEffectBase> ActiveStrategyEffect;

	/**
	 * @brief 计谋卡实例 ID
	 * @details 用于在确认时使用卡牌
	 */
	FGuid StrategyCardInstanceId;

public:
	UFUNCTION(BlueprintCallable, Category = "Card")
	USG_CardDeckComponent* GetCardDeckComponent() const;

	// ========== 放置系统函数 ==========
	
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void StartCardPlacement(USG_CardDataBase* CardData, const FGuid& CardInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void ConfirmPlacement();

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void CancelPlacement();

	// ========== ✨ 新增 - 通用计谋卡接口（低耦合）==========
	
	/**
	 * @brief 开始计谋卡目标选择
	 * @param StrategyCardData 计谋卡数据
	 * @param CardInstanceId 卡牌实例 ID
	 * @return 是否成功开始
	 * @details
	 * 功能说明：
	 * - 根据卡牌数据中配置的效果类生成效果 Actor
	 * - 调用效果 Actor 的 StartTargetSelection
	 * - 效果类自己负责预览显示
	 * 注意事项：
	 * - PlayerController 不关心具体是什么类型的计谋
	 * - 所有特定逻辑由效果类自己实现
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy", meta = (DisplayName = "开始计谋目标选择"))
	bool StartStrategyTargetSelection(USG_StrategyCardData* StrategyCardData, const FGuid& CardInstanceId);

	/**
	 * @brief 确认计谋目标
	 * @return 是否成功确认
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy", meta = (DisplayName = "确认计谋目标"))
	bool ConfirmStrategyTarget();

	/**
	 * @brief 取消计谋目标选择
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy", meta = (DisplayName = "取消计谋选择"))
	void CancelStrategyTargetSelection();

	/**
	 * @brief 直接使用计谋卡（不需要选择目标）
	 * @param StrategyCardData 计谋卡数据
	 * @param CardInstanceId 卡牌实例 ID
	 */
	UFUNCTION(BlueprintCallable, Category = "Strategy")
	void UseStrategyCardDirectly(USG_StrategyCardData* StrategyCardData, const FGuid& CardInstanceId);

	/**
	 * @brief 检查是否正在选择计谋目标
	 */
	UFUNCTION(BlueprintPure, Category = "Strategy", meta = (DisplayName = "是否正在选择计谋目标"))
	bool IsSelectingStrategyTarget() const { return CurrentPlacementMode == ESGPlacementMode::StrategyTarget; }

	/**
	 * @brief 获取当前放置模式
	 */
	UFUNCTION(BlueprintPure, Category = "Placement", meta = (DisplayName = "获取放置模式"))
	ESGPlacementMode GetCurrentPlacementMode() const { return CurrentPlacementMode; }

	/**
	 * @brief 检查卡牌是否需要放置预览
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	bool DoesCardRequirePreview(USG_CardDataBase* CardData) const;

private:
	void BindPawnInputEvents();
	
	UFUNCTION()
	void OnConfirmInput();

	UFUNCTION()
	void OnCancelInput();

	void SpawnUnitFromCard(USG_CardDataBase* CardData, const FVector& UnitSpawnLocation, const FRotator& UnitSpawnRotation);

	UFUNCTION()
	void OnCardSelectionChanged(const FGuid& SelectedId);

	// ✨ 新增 - 计谋效果完成回调
	/**
	 * @brief 计谋效果完成回调
	 * @param Effect 效果 Actor
	 * @param bSuccess 是否成功
	 */
	UFUNCTION()
	void OnStrategyEffectFinished(ASG_StrategyEffectBase* Effect, bool bSuccess);

	ASG_MainCityBase* FindEnemyMainCity();
	FRotator CalculateUnitSpawnRotation(const FVector& UnitLocation);

	/**
	 * @brief 获取鼠标在地面的世界位置
	 */
	bool GetMouseGroundLocation(FVector& OutLocation) const;
    
	UPROPERTY(Transient)
	TObjectPtr<ASG_MainCityBase> CachedEnemyMainCity = nullptr;



	
};