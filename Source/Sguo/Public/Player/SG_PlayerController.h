// 🔧 MODIFIED FILE - 玩家控制器头文件
// Copyright notice placeholder
/**
 * @file SG_PlayerController.h
 * @brief 玩家控制器
 */

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

UCLASS()
class SGUO_API ASG_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASG_PlayerController();

protected:
	virtual void BeginPlay() override;
	
	// ✨ NEW - 设置输入模式
	/**
	 * @brief 设置输入模式
	 * @details
	 * 功能说明：
	 * - 绑定 Pawn 的输入事件
	 * - 监听确认和取消输入
	 */
	virtual void SetupInputComponent() override;

	// ✨ NEW - 重写 OnPossess 以在 Pawn 就绪后绑定事件
	/**
	 * @brief 当控制器占有 Pawn 时调用
	 * @param InPawn 被占有的 Pawn
	 * @details
	 * 功能说明：
	 * - 在 Pawn 被占有后立即绑定输入事件
	 * - 确保 GetPawn() 返回有效指针
	 * 注意事项：
	 * - 此函数在 SetupInputComponent 之后调用
	 * - 是绑定 Pawn 特定事件的最佳时机
	 */
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
	
	// ✨ NEW - 预览 Actor 类
	/**
	 * @brief 放置预览 Actor 类
	 * @details 在蓝图中设置，用于显示卡牌放置预览
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ASG_PlacementPreview> PlacementPreviewClass;

	// ✨ NEW - 当前预览 Actor 实例
	/**
	 * @brief 当前生成的预览 Actor
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ASG_PlacementPreview> CurrentPreviewActor;

	// ✨ NEW - 当前选中的卡牌数据
	/**
	 * @brief 当前选中的卡牌数据
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Placement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USG_CardDataBase> CurrentSelectedCardData;

	// ✨ NEW - 当前选中的卡牌实例 ID
	/**
	 * @brief 当前选中的卡牌实例 ID
	 */
	FGuid CurrentSelectedCardInstanceId;

	
	// ✨ NEW - 标记是否已绑定 Pawn 事件
	/**
	 * @brief 是否已绑定 Pawn 输入事件
	 * @details 防止重复绑定
	 */
	bool bPawnInputBound = false;

public:
	UFUNCTION(BlueprintCallable, Category = "Card")
	USG_CardDeckComponent* GetCardDeckComponent() const;

	// ========== 放置系统函数 ==========
	
	// ✨ NEW - 开始放置卡牌
	/**
	 * @brief 开始放置卡牌
	 * @param CardData 卡牌数据
	 * @param CardInstanceId 卡牌实例 ID
	 * @details
	 * 功能说明：
	 * - 生成预览 Actor
	 * - 开始跟随鼠标
	 * 详细流程：
	 * 1. 检查是否已有预览 Actor
	 * 2. 生成新的预览 Actor
	 * 3. 初始化预览 Actor
	 * 4. 保存卡牌数据和实例 ID
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void StartCardPlacement(USG_CardDataBase* CardData, const FGuid& CardInstanceId);

	// ✨ NEW - 确认放置
	/**
	 * @brief 确认放置卡牌
	 * @details
	 * 功能说明：
	 * - 在预览位置生成单位
	 * - 使用卡牌
	 * - 销毁预览 Actor
	 * 详细流程：
	 * 1. 检查是否可以放置
	 * 2. 获取预览位置
	 * 3. 生成单位
	 * 4. 使用卡牌（进入冷却）
	 * 5. 销毁预览 Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void ConfirmPlacement();

	// ✨ NEW - 取消放置
	/**
	 * @brief 取消放置卡牌
	 * @details
	 * 功能说明：
	 * - 销毁预览 Actor
	 * - 取消选中卡牌
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void CancelPlacement();

private:
	// ✨ NEW - 绑定 Pawn 输入事件
	/**
	 * @brief 绑定 Pawn 的输入事件
	 * @details
	 * 功能说明：
	 * - 监听 Pawn 的确认和取消输入
	 * - 防止重复绑定
	 */
	void BindPawnInputEvents();
	
	// ✨ NEW - 监听 Pawn 的确认输入
	/**
	 * @brief 处理确认输入（右键）
	 */
	UFUNCTION()
	void OnConfirmInput();

	// ✨ NEW - 监听 Pawn 的取消输入
	/**
	 * @brief 处理取消输入（左键）
	 */
	UFUNCTION()
	void OnCancelInput();

	// 🔧 MODIFIED - 修改参数名避免与基类成员变量冲突
	/**
	 * @brief 根据卡牌数据生成单位
	 * @param CardData 卡牌数据
	 * @param UnitSpawnLocation 单位生成位置（修改名称避免冲突）
	 * @param UnitSpawnRotation 单位生成旋转（修改名称避免冲突）
	 */
	void SpawnUnitFromCard(USG_CardDataBase* CardData, const FVector& UnitSpawnLocation, const FRotator& UnitSpawnRotation);
	// ✨ NEW - 监听卡组选中变化
	/**
	 * @brief 监听卡组选中变化
	 * @param SelectedId 选中的卡牌实例 ID
	 * @details
	 * 功能说明：
	 * - 当卡牌被选中时，开始放置流程
	 * - 当卡牌被取消选中时，取消放置
	 */
	UFUNCTION()
	void OnCardSelectionChanged(const FGuid& SelectedId);



	// ✨ NEW - 查找敌方主城
	/**
	 * @brief 查找敌方主城
	 * @return 敌方主城 Actor，如果未找到返回 nullptr
	 * @details
	 * 功能说明：
	 * - 在场景中查找敌方主城
	 * - 用于计算单位朝向
	 * 详细流程：
	 * 1. 获取场景中所有主城
	 * 2. 筛选敌方阵营的主城
	 * 3. 返回第一个找到的敌方主城
	 * 注意事项：
	 * - 结果会被缓存，避免重复查找
	 */
	ASG_MainCityBase* FindEnemyMainCity();
    
	// ✨ NEW - 计算单位生成朝向
	/**
	 * @brief 计算单位生成朝向
	 * @param SpawnLocation 生成位置
	 * @return 朝向旋转
	 * @details
	 * 功能说明：
	 * - 根据敌方主城位置计算朝向
	 * - 如果未找到敌方主城，朝向 +X 方向
	 */
	FRotator CalculateUnitSpawnRotation(const FVector& UnitLocation);
    
	// ✨ NEW - 缓存的敌方主城引用
	/**
	 * @brief 缓存的敌方主城引用
	 * @details 避免每次都查找
	 */
	UPROPERTY(Transient)
	TObjectPtr<ASG_MainCityBase> CachedEnemyMainCity = nullptr;
};
