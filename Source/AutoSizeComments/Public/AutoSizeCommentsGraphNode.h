// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutoSizeCommentsMacros.h"
#include "SGraphNode.h"

class SHorizontalBox;
class SButton;
enum class EASCResizingMode : uint8;
enum class ECommentCollisionMethod : uint8;
class SCommentBubble;
class UAutoSizeCommentsSettings;
struct FASCCommentData;
struct FPresetCommentStyle;
class UEdGraphNode_Comment;

DECLARE_STATS_GROUP(TEXT("AutoSizeComments"), STATGROUP_AutoSizeComments, STATCAT_Advanced);

/**
 * Auto resizing comment node
 */
enum class EASCAnchorPoint : uint8
{
	Left,
	Right,
	Top,
	Bottom,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	None
};

class SAutoSizeCommentsGraphNode final : public SGraphNode
{
public:
	uint8 TwoPassResizeDelay = 0;

	bool bIsDragging = false;

	bool bIsMoving = false;

	bool bPreviousAltDown = false;

	/** Variables related to resizing the comment box by dragging anchor corner points */
	FASCVector2 DragSize;
	bool bUserIsDragging = false;

	EASCAnchorPoint CachedAnchorPoint = EASCAnchorPoint::None;

	bool bWasCopyPasted = false;

	bool bRequireUpdate = false;

	virtual void MoveTo(const FASCVector2& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;

public:
	// @formatter:off
	SLATE_BEGIN_ARGS(SAutoSizeCommentsGraphNode) {}
	SLATE_END_ARGS()
	// @formatter:on

	void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);
	virtual ~SAutoSizeCommentsGraphNode() override;
	void OnDeleted();

	//~ Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override { return FReply::Unhandled(); }
	//~ End SWidget Interface

	//~ Begin SNodePanel::SNode Interface
	virtual bool ShouldAllowCulling() const override { return false; }
	virtual int32 GetSortDepth() const override;
	//~ End SNodePanel::SNode Interface

	//~ Begin SPanel Interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	//~ End SPanel Interface

	//~ Begin SGraphNode Interface
	virtual bool IsNameReadOnly() const override;
	virtual FSlateColor GetCommentColor() const override { return GetCommentBodyColor(); }
	//~ End SGraphNode Interface

	/** return if the node can be selected, by pointing given location */
	virtual bool CanBeSelected(const FASCVector2& MousePositionInNode) const override;

	/** return size of the title bar */
#if ASC_UE_VERSION_OR_LATER(5, 6)
	virtual FASCVector2 GetDesiredSizeForMarquee2f() const override;
#else
	virtual FASCVector2 GetDesiredSizeForMarquee() const override;
#endif

	/** return rect of the title bar */
	virtual FSlateRect GetTitleRect() const override;

	FASCVector2 GetPos() const;

	class UEdGraphNode_Comment* GetCommentNodeObj() const { return CommentNode; }

	FASCCommentData& GetCommentData() const;

	void ResizeToFit();
	void ResizeToFit_Impl();

	void ApplyHeaderStyle();
	void ApplyPresetStyle(const FPresetCommentStyle& Style);

	void OnTitleChanged(const FString& OldTitle, const FString& NewTitle);

protected:
	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;

	//~ Begin SNodePanel::SNode Interface
	virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	//~ End SNodePanel::SNode Interface

	FReply HandleRandomizeColorButtonClicked();
	FReply HandleResizeButtonClicked();
	FReply HandleHeaderButtonClicked();
	FReply HandleRefreshButtonClicked();
	FReply HandlePresetButtonClicked(const FPresetCommentStyle Style);
	FReply HandleAddButtonClicked();
	FReply HandleSubtractButtonClicked();
	FReply HandleClearButtonClicked();

	void InitializeASCNode(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes);
	void InitializeNodesUnderComment(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes);
	void InitialDetectNodes();

	bool AddAllSelectedNodes(bool bExpandComments = false);
	bool RemoveAllSelectedNodes(bool bExpandComments = false);

	void UpdateColors(const float InDeltaTime);

private:
	/** @return the color to tint the comment body */
	FSlateColor GetCommentBodyColor() const;

	/** @return the color to tint the title bar */
	FSlateColor GetCommentTitleBarColor() const;

	/** @return the color to tint the comment bubble */
	FSlateColor GetCommentBubbleColor() const;

	FLinearColor CommentControlsColor;
	FSlateColor GetCommentControlsColor() const;

	FLinearColor CommentControlsTextColor;
	FSlateColor GetCommentControlsTextColor() const;

	FSlateColor GetPresetColor(const FLinearColor Color) const;

	/** Returns the width to wrap the text of the comment at */
	float GetWrapAt() const;

	void MoveEmptyCommentBoxes();

	void CreateCommentControls();
	void CreateColorControls();

	void InitializeColor(const UAutoSizeCommentsSettings& ASCSettings, bool bIsPresetStyle, bool bIsHeaderComment);
	void InitializeCommentBubbleSettings();
	void ApplyDefaultCommentColorMethod();

	bool AreControlsEnabled() const;

	FASCVector2 UserSize = FASCVector2(200, 200);

	bool bHasSetNodesUnderComment = false;

	/** the title bar, needed to obtain it's height */
	TSharedPtr<SBorder> TitleBar;

	UEdGraphNode_Comment* CommentNode = nullptr;

	TSharedPtr<SCommentBubble> CommentBubble;

	/** cached comment title */
	FString CachedCommentTitle;

	/** cached comment title */
	int32 CachedWidth = 0;

	/** cached font size */
	int32 CachedFontSize = 0;

	int32 CachedNumPresets = 0;

	bool bCachedBubbleVisibility = false;
	bool bCachedColorCommentBubble = false;

	float OpacityValue = 0;

	bool bIsHeader = false;

	bool bInitialized = false;

	// TODO: Look into resize transaction perhaps requires the EdGraphNode_Comment to have UPROPERTY() for NodesUnderComment
	// TSharedPtr<FScopedTransaction> ResizeTransaction;

	/** Local copy of the comment style */
	FInlineEditableTextBlockStyle CommentStyle;
	FSlateColor GetCommentTextColor() const;

	TSharedPtr<SButton> ResizeButton;
	TSharedPtr<SButton> ToggleHeaderButton;
	TSharedPtr<SHorizontalBox> ColorControls;
	TSharedPtr<SHorizontalBox> CommentControls;

	bool bAreControlsEnabled = false;

	FName CachedGraphClassName;
	FString OldNodeTitle;

public:
	void RefreshNodesInsideComment(const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false, const bool bUpdateExistingComments = true);

	float GetTitleBarHeight() const;

	/** Util functions */
	FSlateRect GetBoundsForNodesInside();
	FSlateRect GetNodeBounds(UEdGraphNode* Node);
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> GetOtherCommentNodes();
	TArray<UEdGraphNode_Comment*> GetParentComments() const;
	void UpdateExistingCommentNodes(const TArray<UEdGraphNode_Comment*>* OldParentComments, const TArray<UObject*>* OldCommentContains);
	void UpdateExistingCommentNodes();
	bool AnySelectedNodes();
	static FSlateRect GetCommentBounds(UEdGraphNode_Comment* InCommentNode);
	void SnapVectorToGrid(FASCVector2& Vector);
	void SnapBoundsToGrid(FSlateRect& Bounds, int GridMultiplier);
	bool IsLocalPositionInCorner(const FASCVector2& MousePositionInNode) const;
	TArray<UEdGraphNode*> GetNodesUnderComment() const;
	bool AddAllNodesUnderComment(const TArray<UObject*>& Nodes, const bool bUpdateExistingComments = true);
	bool IsValidGraphPanel(TSharedPtr<SGraphPanel> GraphPanel);
	void RemoveInvalidNodes();

	EASCAnchorPoint GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

	void SetIsHeader(bool bNewValue, bool bUpdateStyle);
	bool IsHeaderComment() const;
	bool IsPresetStyle();

	bool LoadCache();
	void UpdateCache();

	void QueryNodesUnderComment(TArray<UEdGraphNode*>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false);
	void QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false);

	void RandomizeColor();

	void AdjustMinSize(FASCVector2& InSize);

	bool HasNodeBeenDeleted(UEdGraphNode* Node);

	bool CanAddNode(const TSharedPtr<SGraphNode> OtherGraphNode, const bool bIgnoreKnots = false) const;
	bool CanAddNode(const UObject* Node, const bool bIgnoreKnots = false) const;
	void OnAltReleased();

	static bool IsCommentNode(UObject* Object);
	static bool IsNotCommentNode(UObject* Object) { return !IsCommentNode(Object); }
	static bool IsMajorNode(UObject* Object);
	static bool IsHeaderComment(UEdGraphNode_Comment* OtherComment);

	FKey GetResizeKey() const;
	bool AreResizeModifiersDown(bool bDownIfNoModifiers = true) const;

	bool IsSingleSelectedNode() const;

	bool IsNodeUnrelated() const;
	void SetNodesRelated(const TArray<UEdGraphNode*>& Nodes, bool bIncludeSelf = true);
	void ResetNodesUnrelated();

	bool IsExistingComment() const;

	EASCResizingMode GetResizingMode() const;

	FASCCommentData& GetCommentData();

	EGraphRenderingLOD::Type GetLOD() const;
};
