// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphNode.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsInputProcessor.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsStyle.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "GraphEditorSettings.h"
#include "K2Node_Knot.h"
#include "SCommentBubble.h"
#include "SGraphPanel.h"
#include "TutorialMetaData.h"
#include "Framework/Application/SlateApplication.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "Materials/MaterialExpressionComment.h"
#include "Runtime/Engine/Classes/EdGraph/EdGraph.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
//#include "ScopedTransaction.h"

void SAutoSizeCommentsGraphNode::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
{
	GraphNode = InNode;

	CommentNode = Cast<UEdGraphNode_Comment>(InNode);

	if (UEdGraph* Graph = InNode->GetGraph())
	{
		if (UClass* GraphClass = Graph->GetClass())
		{
			CachedGraphClassName = GraphClass->GetFName();
		}
	}

	CachedCommentTitle = GetNodeComment();

	// check if we are a header comment
	bIsHeader = GetCommentData().IsHeader();

	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	const bool bIsPresetStyle = IsPresetStyle();

	// use default font
	if (ASCSettings.bUseDefaultFontSize && !bIsHeader && !bIsPresetStyle)
	{
		CommentNode->FontSize = ASCSettings.DefaultFontSize;
	}

	bCachedBubbleVisibility = CommentNode->bCommentBubbleVisible;
	bCachedColorCommentBubble = CommentNode->bColorCommentBubble;

	// Set widget colors
	OpacityValue = ASCSettings.MinimumControlOpacity;
	CommentControlsTextColor = FLinearColor(1, 1, 1, OpacityValue);
	CommentControlsColor = FLinearColor(CommentNode->CommentColor.R, CommentNode->CommentColor.G, CommentNode->CommentColor.B, OpacityValue);

	// Pull out sizes
	UserSize.X = InNode->NodeWidth;
	UserSize.Y = InNode->NodeHeight;

	UpdateGraphNode();
}

SAutoSizeCommentsGraphNode::~SAutoSizeCommentsGraphNode()
{
	if (!bInitialized)
	{
		return;
	}

	if (FASCState::Get().GetASCComment(CommentNode).Get() == this)
	{
		FASCState::Get().RemoveComment(CommentNode);
	}
}

void SAutoSizeCommentsGraphNode::OnDeleted()
{
}

void SAutoSizeCommentsGraphNode::InitializeColor(const UAutoSizeCommentsSettings& ASCSettings, const bool bIsPresetStyle, const bool bIsHeaderComment)
{
	if (bIsHeaderComment)
	{
		return;
	}

	// don't need to initialize if our color is a preset style
	if (bIsPresetStyle)
	{
		return;
	}

	// Set comment color
	const FLinearColor& ASCDefaultColor = ASCSettings.DefaultCommentColor;
	const FLinearColor& EditorDefaultColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	switch (ASCSettings.DefaultCommentColorMethod)
	{
		case EASCDefaultCommentColorMethod::Random:
		{
			// only randomize if the node has the default color
			if (CommentNode->CommentColor == ASCDefaultColor || CommentNode->CommentColor == EditorDefaultColor)
			{
				RandomizeColor();
			}

			break;
		}
		case EASCDefaultCommentColorMethod::Default:
		{
			// use the ASC default color
			if (ASCSettings.bAggressivelyUseDefaultColor || CommentNode->CommentColor == EditorDefaultColor)
			{
				CommentNode->CommentColor = ASCDefaultColor;
			}

			break;
		}
		default: ;
	}
}

void SAutoSizeCommentsGraphNode::ApplyDefaultCommentColorMethod()
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();
	switch (ASCSettings.DefaultCommentColorMethod)
	{
		case EASCDefaultCommentColorMethod::Random:
		{
			RandomizeColor();
			break;
		}
		case EASCDefaultCommentColorMethod::Default:
		{
			CommentNode->CommentColor = ASCSettings.DefaultCommentColor;
			break;
		}
		default: ;
	}
}

void SAutoSizeCommentsGraphNode::InitializeCommentBubbleSettings()
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	if (ASCSettings.bEnableCommentBubbleDefaults)
	{
		CommentNode->bColorCommentBubble = ASCSettings.bDefaultColorCommentBubble;
		CommentNode->bCommentBubbleVisible_InDetailsPanel = ASCSettings.bDefaultShowBubbleWhenZoomed;
		CommentNode->bCommentBubblePinned = ASCSettings.bDefaultShowBubbleWhenZoomed;
		CommentNode->bCommentBubbleVisible = ASCSettings.bDefaultShowBubbleWhenZoomed;
	}
}

void SAutoSizeCommentsGraphNode::MoveTo(const FASCVector2& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
{
	/** Copied from SGraphNodeComment::MoveTo */
	if (!bIsMoving)
	{
		if (UAutoSizeCommentsSettings::Get().bRefreshContainingNodesOnMove)
		{
			RefreshNodesInsideComment(ECommentCollisionMethod::Contained);
			bIsMoving = true;
		}
	}

	const FASCVector2 PositionDelta = NewPosition - GetPos();
	SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();

	const ECommentCollisionMethod AltCollisionMethod = UAutoSizeCommentsSettings::Get().AltCollisionMethod;
	if (KeysState.IsAltDown() && AltCollisionMethod != ECommentCollisionMethod::Disabled)
	{
		// still update collision when we alt-control drag
		TArray<UEdGraphNode*> NodesUnderComment;
		QueryNodesUnderComment(NodesUnderComment, AltCollisionMethod);
		SetNodesRelated(NodesUnderComment);
	}
	else if (IsSingleSelectedNode())
	{
		if (UAutoSizeCommentsSettings::Get().bHighlightContainingNodesOnSelection)
		{
			const TArray<UEdGraphNode*> NodesUnderComment = GetNodesUnderComment();
			SetNodesRelated(NodesUnderComment);
		}
	}

	if (!(KeysState.IsAltDown() && KeysState.IsControlDown()) && !IsHeaderComment())
	{
		if (CommentNode && CommentNode->MoveMode == ECommentBoxMode::GroupMovement)
		{
			// Now update any nodes which are touching the comment but *not* selected
			// Selected nodes will be moved as part of the normal selection code
			TSharedPtr<SGraphPanel> Panel = GetOwnerPanel();

			for (FCommentNodeSet::TConstIterator NodeIt(CommentNode->GetNodesUnderComment()); NodeIt; ++NodeIt)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
				{
					if (!Panel->SelectionManager.IsNodeSelected(Node))
					{
						if (TSharedPtr<SGraphNode> PanelGraphNode = FASCUtils::GetGraphNode(Panel, Node))
						{
							if (!NodeFilter.Find(PanelGraphNode))
							{
								NodeFilter.Add(PanelGraphNode);
#if ASC_UE_VERSION_OR_LATER(4, 27)
								Node->Modify(bMarkDirty);
#else
								Node->Modify();
#endif
								Node->NodePosX += PositionDelta.X;
								Node->NodePosY += PositionDelta.Y;
							}
						}
					}
				}
			}
		}
	}

	// from SGraphNodeMaterialComment
	if (UMaterialGraphNode_Comment* MaterialComment = Cast<UMaterialGraphNode_Comment>(CommentNode))
	{
		MaterialComment->MaterialExpressionComment->MaterialExpressionEditorX = CommentNode->NodePosX;
		MaterialComment->MaterialExpressionComment->MaterialExpressionEditorY = CommentNode->NodePosY;
		MaterialComment->MaterialExpressionComment->MarkPackageDirty();
		MaterialComment->MaterialDirtyDelegate.ExecuteIfBound();
	}
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsEditable.Get())
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.GetEffectingButton() == GetResizeKey() && AreResizeModifiersDown())
	{
		CachedAnchorPoint = GetAnchorPoint(MyGeometry, MouseEvent);
		if (CachedAnchorPoint != EASCAnchorPoint::None)
		{
			DragSize = UserSize;
			bUserIsDragging = true;

			// deselect all nodes when we are trying to resize
			GetOwnerPanel()->SelectionManager.ClearSelectionSet();

			// ResizeTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "Resize Comment Node", "Resize Comment Node")));
			// CommentNode->Modify();
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FASCVector2 MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (CanBeSelected(MousePositionInNode))
		{
			if (UAutoSizeCommentsSettings::Get().bRefreshContainingNodesOnMove)
			{
				bIsMoving = false;
			}
		}
	}

	return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUserIsDragging)
	{
		ResetNodesUnrelated();
	}

	if ((MouseEvent.GetEffectingButton() == GetResizeKey()) && bUserIsDragging)
	{
		bUserIsDragging = false;
		CachedAnchorPoint = EASCAnchorPoint::None;
		RefreshNodesInsideComment(UAutoSizeCommentsSettings::Get().ResizeCollisionMethod, UAutoSizeCommentsSettings::Get().bIgnoreKnotNodesWhenResizing);

		if (UAutoSizeCommentsSettings::Get().ShouldResizeToFit())
		{
			ResizeToFit();
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	return SGraphNode::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUserIsDragging)
	{
		static const FASCVector2 Padding(5, 5);
		FASCVector2 MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		MousePositionInNode.X = FMath::RoundToInt(MousePositionInNode.X);
		MousePositionInNode.Y = FMath::RoundToInt(MousePositionInNode.Y);

		int32 OldNodeWidth = GraphNode->NodeWidth;
		int32 OldNodeHeight = GraphNode->NodeHeight;

		FASCVector2 NewSize = UserSize;

		bool bAnchorLeft = false;
		bool bAnchorTop = false;

		// LEFT
		if (CachedAnchorPoint == EASCAnchorPoint::Left || CachedAnchorPoint == EASCAnchorPoint::TopLeft || CachedAnchorPoint == EASCAnchorPoint::BottomLeft)
		{
			bAnchorLeft = true;
			NewSize.X -= MousePositionInNode.X - Padding.X;
		}

		// RIGHT
		if (CachedAnchorPoint == EASCAnchorPoint::Right || CachedAnchorPoint == EASCAnchorPoint::TopRight || CachedAnchorPoint == EASCAnchorPoint::BottomRight)
		{
			NewSize.X = MousePositionInNode.X + Padding.X;
		}

		// TOP
		if (CachedAnchorPoint == EASCAnchorPoint::Top || CachedAnchorPoint == EASCAnchorPoint::TopLeft || CachedAnchorPoint == EASCAnchorPoint::TopRight)
		{
			bAnchorTop = true;
			NewSize.Y -= MousePositionInNode.Y - Padding.Y;
		}

		// BOTTOM
		if (CachedAnchorPoint == EASCAnchorPoint::Bottom || CachedAnchorPoint == EASCAnchorPoint::BottomLeft || CachedAnchorPoint == EASCAnchorPoint::BottomRight)
		{
			NewSize.Y = MousePositionInNode.Y + Padding.Y;
		}

		AdjustMinSize(NewSize);

		if (UAutoSizeCommentsSettings::Get().bSnapToGridWhileResizing)
		{
			SnapVectorToGrid(NewSize);
		}

		if (UserSize != NewSize)
		{
			UserSize = NewSize;

			GetNodeObj()->ResizeNode(UserSize);

			if (bAnchorLeft)
			{
				int32 DeltaWidth = GraphNode->NodeWidth - OldNodeWidth;
				GraphNode->NodePosX -= DeltaWidth;
			}

			if (bAnchorTop)
			{
				int32 DeltaHeight = GraphNode->NodeHeight - OldNodeHeight;
				GraphNode->NodePosY -= DeltaHeight;
			}
		}

#if ASC_UE_VERSION_OR_LATER(4, 23)
		TArray<UEdGraphNode*> Nodes;
		QueryNodesUnderComment(Nodes, UAutoSizeCommentsSettings::Get().ResizeCollisionMethod);
		SetNodesRelated(Nodes);
#endif
	}

	return SGraphNode::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	/** Copied from SGraphNodeComment::OnMouseButtonDoubleClick */
	FASCVector2 LocalMouseCoordinates = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	// If user double-clicked in the title bar area
	if (LocalMouseCoordinates.Y < GetTitleBarHeight())
	{
		// Request a rename
		RequestRename();

		// Set the keyboard focus
		if (!HasKeyboardFocus())
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}

		return FReply::Handled();
	}
	// Otherwise let the graph handle it, to allow spline interactions to work when they overlap with a comment node
	return FReply::Unhandled();
}

void SAutoSizeCommentsGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::Tick"), STAT_ASC_Tick, STATGROUP_AutoSizeComments);

	if (!bInitialized)
	{
		// if we are not initialized we are most likely a preview node, pull size from the comment 
		UserSize.X = CommentNode->NodeWidth;
		UserSize.Y = CommentNode->NodeHeight;
		return;
	}

	if (FASCUtils::IsGraphReadOnly(GetOwnerPanel()))
	{
		return;
	}

	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	bAreControlsEnabled = !AreResizeModifiersDown(false) && (!UAutoSizeCommentsSettings::Get().EnableCommentControlsKey.Key.IsValid() || bAreControlsEnabled);

	// We need to call this on tick since there are quite a few methods of deleting
	// nodes without any callbacks (undo, collapse to function / macro...)
	RemoveInvalidNodes();

	const EASCResizingMode ResizingMode = GetResizingMode();

	if (ResizingMode == EASCResizingMode::Disabled)
	{
		UserSize.X = CommentNode->NodeWidth;
		UserSize.Y = CommentNode->NodeHeight;
	}

	if (TwoPassResizeDelay > 0)
	{
		if (--TwoPassResizeDelay == 0)
		{
			ResizeToFit_Impl();
		}
	}

	if (!IsHeaderComment() && !bUserIsDragging)
	{
		const FModifierKeysState& KeysState = FSlateApplication::Get().GetModifierKeys();

		const bool bIsAltDown = KeysState.IsAltDown();
		if (!bIsAltDown)
		{
			const ECommentCollisionMethod& AltCollisionMethod = UAutoSizeCommentsSettings::Get().AltCollisionMethod;

			// still update collision when we alt-control drag
			const bool bUseAltCollision = AltCollisionMethod != ECommentCollisionMethod::Disabled;

			// refresh when the alt key is released
			if (bPreviousAltDown && bUseAltCollision)
			{
				OnAltReleased();
			}

			if (ResizingMode == EASCResizingMode::Always)
			{
				ResizeToFit();
			}
			else if (ResizingMode == EASCResizingMode::Reactive &&
				FAutoSizeCommentGraphHandler::Get().HasCommentChanged(CommentNode))
			{
				FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode);
				ResizeToFit();
			}

			MoveEmptyCommentBoxes();

			// if (ResizeTransaction.IsValid())
			// {
			// 	ResizeTransaction.Reset();
			// }
		}

		bPreviousAltDown = bIsAltDown;
	}

	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (IsHeaderComment())
	{
		UserSize.Y = GetTitleBarHeight();
		CommentNode->NodeHeight = UserSize.Y;
	}

	// Update cached title
	const FString CurrentCommentTitle = GetNodeComment();
	if (CurrentCommentTitle != CachedCommentTitle)
	{
		OnTitleChanged(CachedCommentTitle, CurrentCommentTitle);
		CachedCommentTitle = CurrentCommentTitle;
	}

	// Update cached width
	const int32 CurrentWidth = static_cast<int32>(UserSize.X);
	if (CurrentWidth != CachedWidth)
	{
		CachedWidth = CurrentWidth;
	}

	// Otherwise update when cached values have changed
	if (bCachedBubbleVisibility != CommentNode->bCommentBubbleVisible_InDetailsPanel)
	{
		bCachedBubbleVisibility = CommentNode->bCommentBubbleVisible_InDetailsPanel;
		if (CommentBubble.IsValid())
		{
			CommentBubble->UpdateBubble();
		}
	}

	// Update cached font size
	if (CachedFontSize != CommentNode->FontSize)
	{
		bRequireUpdate = true;
	}

	if (CachedNumPresets != ASCSettings.PresetStyles.Num())
	{
		bRequireUpdate = true;
	}

	if (bRequireUpdate)
	{
		UpdateGraphNode();
		bRequireUpdate = false;
	}

	UpdateColors(InDeltaTime);
}

void SAutoSizeCommentsGraphNode::UpdateGraphNode()
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	// No pins in a comment box
	InputPins.Empty();
	OutputPins.Empty();

	// Avoid standard box model too
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	SetupErrorReporting();

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);

	if (UAutoSizeCommentsSettings::Get().bUseMinimalTitlebarStyle && ASC_UE_VERSION_OR_LATER(5, 0))
	{
		CommentStyle = FASCStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("ASC.CommentTitleTextBoxStyle");
	}
	else
	{
		CommentStyle = ASC_STYLE_CLASS::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.CommentBlock.TitleInlineEditableText");
	}

#if ASC_UE_VERSION_OR_LATER(5, 1)
	CommentStyle.EditableTextBoxStyle.TextStyle.Font.Size = CommentNode->FontSize;
#else
	CommentStyle.EditableTextBoxStyle.Font.Size = CommentNode->FontSize;
#endif
	CommentStyle.TextStyle.Font.Size = CommentNode->FontSize;
	CachedFontSize = CommentNode->FontSize;

	// Create comment bubble
	if (!ASCSettings.bHideCommentBubble)
	{
		CommentBubble = SNew(SCommentBubble)
			.GraphNode(GraphNode)
			.Text(this, &SAutoSizeCommentsGraphNode::GetNodeComment)
			.OnTextCommitted(this, &SAutoSizeCommentsGraphNode::OnNameTextCommited)
			.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentBubbleColor)
			.AllowPinning(true)
			.EnableTitleBarBubble(false)
			.EnableBubbleCtrls(false)
			.GraphLOD(this, &SAutoSizeCommentsGraphNode::GetLOD)
			.InvertLODCulling(true)
			.IsGraphNodeHovered(this, &SGraphNode::IsHovered);

		GetOrAddSlot(ENodeZone::TopCenter)
#if ASC_UE_VERSION_OR_LATER(5, 6)
			.SlotOffset2f(TAttribute<FASCVector2>(CommentBubble.Get(), &SCommentBubble::GetOffset2f))
			.SlotSize2f(TAttribute<FASCVector2>(CommentBubble.Get(), &SCommentBubble::GetSize2f))
#else
			.SlotOffset(TAttribute<FASCVector2>(CommentBubble.Get(), &SCommentBubble::GetOffset))
			.SlotSize(TAttribute<FASCVector2>(CommentBubble.Get(), &SCommentBubble::GetSize))
#endif
			.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
			.VAlign(VAlign_Top)
			[
				CommentBubble.ToSharedRef()
			];
	}

	// Create the toggle header button
	ToggleHeaderButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Toggle between a header node and a resizing node"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("H")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	const auto MakeAnchorBox = []()
	{
		return SNew(SBox).WidthOverride(16).HeightOverride(16).Visibility(EVisibility::Visible)
		[
			SNew(SBorder).BorderImage(FASCStyle::Get().GetBrush("ASC.AnchorBox"))
		];
	};

	const bool bHideCornerPoints = ASCSettings.bHideCornerPoints;

	CreateCommentControls();

	CreateColorControls();

	ETextJustify::Type CommentTextAlignment = ASCSettings.CommentTextAlignment;

	TSharedRef<SInlineEditableTextBlock> CommentTextBlock = SAssignNew(InlineEditableText, SInlineEditableTextBlock)
		.Style(&CommentStyle)
		.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentTextColor)
		.Text(this, &SAutoSizeCommentsGraphNode::GetEditableNodeTitleAsText)
		.OnVerifyTextChanged(this, &SAutoSizeCommentsGraphNode::OnVerifyNameTextChanged)
		.OnTextCommitted(this, &SAutoSizeCommentsGraphNode::OnNameTextCommited)
		.IsReadOnly(this, &SAutoSizeCommentsGraphNode::IsNameReadOnly)
		.IsSelected(this, &SAutoSizeCommentsGraphNode::IsSelectedExclusively)
		.WrapTextAt(this, &SAutoSizeCommentsGraphNode::GetWrapAt)
		.MultiLine(true)
		.ModiferKeyForNewLine(EModifierKey::Shift)
		.Justification(CommentTextAlignment)
#if ASC_UE_VERSION_OR_LATER(5, 6)
		.DelayedLeftClickEntersEditMode(false)
#endif
		; // ending semicolon because of macro (is there a nicer way of doing this?)

	// Create the top horizontal box containing anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> TopHBox = SNew(SHorizontalBox);

	if (!bHideCornerPoints)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Top).AttachWidget(MakeAnchorBox());
	}

	FMargin CommentTextPadding = ASCSettings.CommentTextPadding;
	TopHBox->AddSlot().Padding(CommentTextPadding).FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(CommentTextBlock);

	if (!ASCSettings.bHideHeaderButton)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(ToggleHeaderButton.ToSharedRef());
	}

	if (!bHideCornerPoints)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(MakeAnchorBox());
	}

	// Create the bottom horizontal box containing comment controls and anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> BottomHBox = SNew(SHorizontalBox);
	if (!IsHeaderComment())
	{ 
		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AttachWidget(MakeAnchorBox());
		}

		if (!ASCSettings.bHideCommentBoxControls)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill).AttachWidget(CommentControls.ToSharedRef());
		}

		BottomHBox->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Bottom).AttachWidget(MakeAnchorBox());
		}
	}

	// Create the title bar
	SAssignNew(TitleBar, SBorder)
		.BorderImage(ASC_STYLE_CLASS::Get().GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(this, &SAutoSizeCommentsGraphNode::GetCommentTitleBarColor)
		.HAlign(HAlign_Fill).VAlign(VAlign_Top)
		[
			TopHBox
		];

	if (!UAutoSizeCommentsSettings::Get().bDisableTooltip)
	{
		TitleBar->SetToolTipText(TAttribute<FText>(this, &SGraphNode::GetNodeTooltip));
	}

	// Create the main vertical box containing all the widgets
	auto MainVBox = SNew(SVerticalBox);
	MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(TitleBar.ToSharedRef());
	MainVBox->AddSlot().AutoHeight().Padding(1.0f).AttachWidget(ErrorReporting->AsWidget());
	if (!IsHeaderComment() && (!ASCSettings.bHidePresets || !UAutoSizeCommentsSettings::Get().bHideRandomizeButton))
	{
		MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(ColorControls.ToSharedRef());
	}
	MainVBox->AddSlot().FillHeight(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

	MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).AttachWidget(BottomHBox);

	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(ASC_STYLE_CLASS::Get().GetBrush("Kismet.Comment.Background"))
		.ColorAndOpacity(FLinearColor::White)
		.BorderBackgroundColor(this, &SAutoSizeCommentsGraphNode::GetCommentBodyColor)
		.AddMetaData<FGraphNodeMetaData>(TagMeta)
		[
			MainVBox
		]
	];
}

FVector2D SAutoSizeCommentsGraphNode::ComputeDesiredSize(float) const
{
#if ASC_UE_VERSION_OR_LATER(5, 6)
	return FVector2D(UserSize);
#else
	return UserSize;
#endif
}

bool SAutoSizeCommentsGraphNode::IsNameReadOnly() const
{
	return !IsEditable.Get() || SGraphNode::IsNameReadOnly();
}

void SAutoSizeCommentsGraphNode::SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel)
{
	SGraphNode::SetOwner(OwnerPanel);

	if (!IsValidGraphPanel(OwnerPanel))
	{
		return;
	}

	TArray<TWeakObjectPtr<UObject>> InitialSelectedNodes;
	for (UObject* SelectedNode : OwnerPanel->SelectionManager.GetSelectedNodes())
	{
		InitialSelectedNodes.Add(SelectedNode);
	}

	// since the graph node is created twice, we need to delay initialization so the correct graph node gets initialized
	const auto InitNode = [](TWeakPtr<SAutoSizeCommentsGraphNode> NodePtr, const TArray<TWeakObjectPtr<UObject>>& SelectedNodes)
	{
		if (NodePtr.IsValid())
		{
			NodePtr.Pin()->InitializeASCNode(SelectedNodes);
		}
	};

	const auto Delegate = FTimerDelegate::CreateLambda(InitNode, SharedThis(this), InitialSelectedNodes);
	GEditor->GetTimerManager()->SetTimerForNextTick(Delegate);
}


void SAutoSizeCommentsGraphNode::InitializeASCNode(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes)
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!CommentNode || !OwnerPanel)
	{
		return;
	}

	TSharedPtr<SGraphNode> NodeWidget = OwnerPanel->GetNodeWidgetFromGuid(CommentNode->NodeGuid);
	if (NodeWidget != AsShared())
	{
		return;
	}

	// if there is already a registered comment do nothing
	if (TSharedPtr<SAutoSizeCommentsGraphNode> RegisteredComment = FASCState::Get().GetASCComment(CommentNode))
	{
		if (RegisteredComment.Get() != this)
		{
			return;
		}
	}

	// if this node is selected then we have been copy pasted, don't add all selected nodes
	bool bHasBeenCopyPasted = InitialSelectedNodes.Contains(CommentNode);

	if (!bInitialized)
	{
		UE_LOG(LogAutoSizeComments, VeryVerbose, TEXT("Init ASC node %p %s %d %d"), this, *CommentNode->NodeGuid.ToString(), IsExistingComment(), bHasBeenCopyPasted);

		bInitialized = true;

		// register graph
		FASCState::Get().RegisterComment(SharedThis(this));

		// init graph handler for containing graph
		FAutoSizeCommentGraphHandler::Get().BindToGraph(CommentNode->GetGraph());

		FAutoSizeCommentGraphHandler::Get().RegisterActiveGraphPanel(GetOwnerPanel());

		InitializeNodesUnderComment(InitialSelectedNodes);

		// make sure to init change state after setting the nodes under comments (if we don't have a state aleady)
		if (!FAutoSizeCommentGraphHandler::Get().HasCommentChangeState(CommentNode))
		{
			FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode);
		}

		FASCCommentData& CommentData = GetCommentData();
		if (!CommentData.HasBeenInitialized())
		{
			CommentData.SetInitialized(true);

			// don't initialize without any selected nodes!
			const bool bShouldApplyColor = !bHasBeenCopyPasted && (!IsExistingComment() || UAutoSizeCommentsSettings::Get().bApplyColorToExistingNodes);
			if (bShouldApplyColor)
			{
				InitializeCommentBubbleSettings();
				InitializeColor(UAutoSizeCommentsSettings::Get(), false, GetCommentData().IsHeader());
			}
		}
	}
}

void SAutoSizeCommentsGraphNode::InitializeNodesUnderComment(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes)
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel)
	{
		return;
	}

	if (!CommentNode)
	{
		return;
	}

	if (IsHeaderComment())
	{
		return;
	}

	LoadCache();

	FASCCommentData& CommentData = GetCommentData();
	if (CommentData.HasBeenInitialized())
	{
		return;
	}

	// check if we actually found anything from the node cache
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		return;
	}

	// if this node is selected then we have been copy pasted, don't add all selected nodes
	if (InitialSelectedNodes.Contains(CommentNode))
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SAutoSizeCommentsGraphNode::InitialDetectNodes));
		return;
	}

	TSet<UObject*> SelectedNodes;
	for (TWeakObjectPtr<UObject> Node : InitialSelectedNodes)
	{
		if (Node.IsValid())
		{
			SelectedNodes.Add(Node.Get());
		}
	}

	// add all selected nodes
	if (SelectedNodes.Num() > 0 && !UAutoSizeCommentsSettings::Get().bIgnoreSelectedNodesOnCreation)
	{
		// if we have selected a comment, also add the nodes inside that comment too
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(*NodeIter))
			{
				SelectedNodes.Append(SelectedComment->GetNodesUnderComment());
			}
		}

		AddAllNodesUnderComment(SelectedNodes.Array());
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SAutoSizeCommentsGraphNode::ResizeToFit));
		return;
	}

	if (UAutoSizeCommentsSettings::Get().bDetectNodesContainedForNewComments)
	{
		// Refresh the nodes under the comment
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SAutoSizeCommentsGraphNode::InitialDetectNodes));
	}
}

void SAutoSizeCommentsGraphNode::InitialDetectNodes()
{
	// possibly could cause some collision issues when the node is initialized offscreen
	// since our bounds (specifically title size) uses GetDesiredSize
	RefreshNodesInsideComment(ECommentCollisionMethod::Point);

	// so that it doesn't trigger the auto resize check
	FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode);

	if (IsExistingComment() && UAutoSizeCommentsSettings::Get().bResizeExistingNodes)
	{
		ResizeToFit();
	}
	// else - we have been copy pasted don't resize

	if (UAutoSizeCommentsSettings::Get().bEnableFixForSortDepthIssue)
	{
		FAutoSizeCommentGraphHandler::Get().RequestGraphVisualRefresh(GetOwnerPanel());
	}
}

bool SAutoSizeCommentsGraphNode::CanBeSelected(const FASCVector2& MousePositionInNode) const
{
	const FASCVector2 Size = GetDesiredSize();
	return MousePositionInNode.X >= 0 && MousePositionInNode.X <= Size.X && MousePositionInNode.Y >= 0 && MousePositionInNode.Y <= GetTitleBarHeight();
}

#if ASC_UE_VERSION_OR_LATER(5, 6)
FASCVector2 SAutoSizeCommentsGraphNode::GetDesiredSizeForMarquee2f() const
#else
FASCVector2 SAutoSizeCommentsGraphNode::GetDesiredSizeForMarquee() const
#endif
{
	return FASCVector2(UserSize.X, GetTitleBarHeight());
}

FCursorReply SAutoSizeCommentsGraphNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ToggleHeaderButton->IsHovered() ||
		ColorControls->IsHovered() ||
		CommentControls->IsHovered() ||
		(ResizeButton && ResizeButton->IsHovered()))
	{
		return FCursorReply::Unhandled();
	}

	if (AreResizeModifiersDown())
	{
		auto AnchorPoint = GetAnchorPoint(MyGeometry, CursorEvent);

		if (AnchorPoint == EASCAnchorPoint::TopLeft || AnchorPoint == EASCAnchorPoint::BottomRight)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
		}

		if (AnchorPoint == EASCAnchorPoint::TopRight || AnchorPoint == EASCAnchorPoint::BottomLeft)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
		}

		if (AnchorPoint == EASCAnchorPoint::Top || AnchorPoint == EASCAnchorPoint::Bottom)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
		}

		if (AnchorPoint == EASCAnchorPoint::Left || AnchorPoint == EASCAnchorPoint::Right)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
	}

	const FASCVector2 LocalMouseCoordinates = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	if (CanBeSelected(LocalMouseCoordinates))
	{
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	return FCursorReply::Unhandled();
}

int32 SAutoSizeCommentsGraphNode::GetSortDepth() const
{
	if (!CommentNode)
	{
		return -1;
	}

	if (IsHeaderComment())
	{
		return 0;
	}

	if (IsSelectedExclusively())
	{
		return 0;
	}

	// Check if mouse is above titlebar for sort depth so comments can be dragged on first click
	const FASCVector2 LocalPos = GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	if (CanBeSelected(LocalPos))
	{
		return 0;
	}

	return CommentNode->CommentDepth;
}

FReply SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked()
{
	RandomizeColor();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleResizeButtonClicked()
{
	ResizeToFit();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked()
{
	SetIsHeader(!bIsHeader, true);
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked()
{
	if (AnySelectedNodes())
	{
		FASCUtils::ClearCommentNodes(CommentNode);
		AddAllSelectedNodes(true);

		if (UAutoSizeCommentsSettings::Get().ShouldResizeToFit())
		{
			ResizeToFit();
		}
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandlePresetButtonClicked(const FPresetCommentStyle Style)
{
	ApplyPresetStyle(Style);

	if (UAutoSizeCommentsSettings::Get().ShouldResizeToFit())
	{
		ResizeToFit();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleAddButtonClicked()
{
	AddAllSelectedNodes(true);

	if (UAutoSizeCommentsSettings::Get().ShouldResizeToFit())
	{
		ResizeToFit();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleSubtractButtonClicked()
{
	RemoveAllSelectedNodes(true);

	if (UAutoSizeCommentsSettings::Get().ShouldResizeToFit())
	{
		ResizeToFit();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleClearButtonClicked()
{
	FASCUtils::ClearCommentNodes(CommentNode);
	UpdateExistingCommentNodes();
	return FReply::Handled();
}

bool SAutoSizeCommentsGraphNode::AddAllSelectedNodes(bool bExpandComments)
{
	bool bDidAddAnything = false;

	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel)
	{
		return false;
	}

	TSet<UEdGraphNode*> NodesToAdd = FASCUtils::GetSelectedNodes(GetOwnerPanel(), bExpandComments);
	for (UEdGraphNode* SelectedObj : NodesToAdd)
	{
		if (CanAddNode(SelectedObj))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, SelectedObj, false);
			bDidAddAnything = true;
		}
	}

	UpdateCache();

	if (bDidAddAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::AddAllNodesUnderComment(const TArray<UObject*>& Nodes, const bool bUpdateExistingComments)
{
	bool bDidAddAnything = false;
	for (UObject* Node : Nodes)
	{
		if (CanAddNode(Node))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, Node);
			bDidAddAnything = true;
		}
	}

	if (bDidAddAnything && bUpdateExistingComments)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::IsValidGraphPanel(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel)
	{
		return false;
	}

	static TArray<FString> InvalidTypes = { "SBlueprintDiff", "SGraphPreviewer" };
	if (FASCUtils::GetParentWidgetOfTypes(GraphPanel, InvalidTypes))
	{
		return false;
	}

	return true;
}

void SAutoSizeCommentsGraphNode::RemoveInvalidNodes()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::RemoveInvalidNodes"), STAT_ASC_RemoveInvalidNodes, STATGROUP_AutoSizeComments);
	const TArray<UObject*>& UnfilteredNodesUnderComment = CommentNode->GetNodesUnderComment();

	const TArray<UEdGraphNode*>& GraphNodes = GetOwnerPanel()->GetGraphObj()->Nodes;

	// Remove all invalid objects
	TSet<UObject*> InvalidObjects;
	for (UObject* Obj : UnfilteredNodesUnderComment)
	{
		// Make sure that we haven't somehow added ourselves
		check(Obj != CommentNode);

		// If a node gets deleted it can still stay inside the comment box
		// So checks if the node is still on the graph
		if (!GraphNodes.Contains(Obj))
		{
			InvalidObjects.Add(Obj);
		}
	}

	FASCUtils::RemoveNodesFromComment(CommentNode, InvalidObjects);
}

bool SAutoSizeCommentsGraphNode::RemoveAllSelectedNodes(bool bExpandComments)
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	bool bDidRemoveAnything = false;

	// const FGraphPanelSelectionSet SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	TSet<UEdGraphNode*> SelectedNodes = FASCUtils::GetSelectedNodes(GetOwnerPanel(), bExpandComments);
	TArray<UEdGraphNode*> NodesUnderComment = GetNodesUnderComment();

	// Clear all nodes under comment
	FASCUtils::ClearCommentNodes(CommentNode, false);

	// Add back the nodes under comment while filtering out any which are selected
	for (UEdGraphNode* NodeUnderComment : NodesUnderComment)
	{
		if (!SelectedNodes.Contains(NodeUnderComment))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, NodeUnderComment, false);
		}
		else
		{
			bDidRemoveAnything = true;
		}
	}

	UpdateCache();

	if (bDidRemoveAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidRemoveAnything;
}

void SAutoSizeCommentsGraphNode::UpdateColors(const float InDeltaTime)
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	const bool bIsCommentControlsKeyDown = ASCSettings.EnableCommentControlsKey.Key.IsValid() && bAreControlsEnabled;

	if (bIsCommentControlsKeyDown || (!ASCSettings.EnableCommentControlsKey.Key.IsValid() && IsHovered()))
	{
		if (OpacityValue < 1.f)
		{
			const float FadeUpAmt = InDeltaTime * 5.f;
			OpacityValue = FMath::Min(OpacityValue + FadeUpAmt, 1.f);
		}
	}
	else
	{
		if (OpacityValue > ASCSettings.MinimumControlOpacity)
		{
			const float FadeDownAmt = InDeltaTime * 5.f;
			OpacityValue = FMath::Max(OpacityValue - FadeDownAmt, ASCSettings.MinimumControlOpacity);
		}
	}

	CommentControlsColor = FLinearColor(CommentNode->CommentColor.R, CommentNode->CommentColor.G, CommentNode->CommentColor.B, OpacityValue);
	CommentControlsTextColor.A = OpacityValue;
}

FSlateRect SAutoSizeCommentsGraphNode::GetTitleRect() const
{
	const FASCVector2 NodePosition = GetPos();
	const FASCVector2 NodeSize = TitleBar.IsValid() ? TitleBar->GetDesiredSize() : GetDesiredSize();
	const FSlateRect TitleBarOffset(13, 8, -3, 0);

	return FSlateRect(NodePosition.X, NodePosition.Y, NodePosition.X + NodeSize.X, NodePosition.Y + NodeSize.Y) + TitleBarOffset;
}

FASCVector2 SAutoSizeCommentsGraphNode::GetPos() const
{
	return FASCUtils::GetNodePos(this);
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentTextColor() const
{
	constexpr FLinearColor TransparentGray(1.0f, 1.0f, 1.0f, 0.4f);
	return IsNodeUnrelated() ? TransparentGray : FLinearColor::White;
}

void SAutoSizeCommentsGraphNode::RefreshNodesInsideComment(const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots, const bool bUpdateExistingComments)
{
	if (OverrideCollisionMethod == ECommentCollisionMethod::Disabled)
	{
		return;
	}

	TArray<UEdGraphNode*> OutNodes;
	QueryNodesUnderComment(OutNodes, OverrideCollisionMethod, bIgnoreKnots);
	OutNodes = OutNodes.FilterByPredicate(IsMajorNode);

	const TSet<UEdGraphNode*> NodesUnderComment(GetNodesUnderComment().FilterByPredicate(IsMajorNode));
	const TSet<UEdGraphNode*> NewNodeSet(OutNodes);


	// nodes inside did not change, do nothing
	if (NodesUnderComment.Num() == NewNodeSet.Num() && NodesUnderComment.Includes(NewNodeSet))
	{
		return;
	}

	FASCUtils::ClearCommentNodes(CommentNode, false);
	for (UEdGraphNode* Node : OutNodes)
	{
		if (CanAddNode(Node, bIgnoreKnots))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, Node, false);
		}
	}

	if (bUpdateExistingComments)
	{
		UpdateExistingCommentNodes();
	}

	UpdateCache();
}

float SAutoSizeCommentsGraphNode::GetTitleBarHeight() const
{
	return TitleBar.IsValid() ? TitleBar->GetDesiredSize().Y : 0.0f;
}

void SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes()
{
	UpdateExistingCommentNodes(nullptr, nullptr);
}

void SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes(const TArray<UEdGraphNode_Comment*>* OldParentComments, const TArray<UObject*>* OldCommentContains)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes"), STAT_ASC_UpdateExistingCommentNodes, STATGROUP_AutoSizeComments);

	// Get list of all other comment nodes
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> OtherCommentNodes = GetOtherCommentNodes();

	TArray<UObject*> OurMainNodes = CommentNode->GetNodesUnderComment().FilterByPredicate(IsMajorNode);

	TArray<UEdGraphNode_Comment*> CurrentParentComments = GetParentComments();

	// Remove ourselves from our parent comments, as we will be adding ourselves later if required
	for (UEdGraphNode_Comment* ParentComment : CurrentParentComments)
	{
		TSet<UObject*> NodesToRemove = { CommentNode };
		FASCUtils::RemoveNodesFromComment(ParentComment, NodesToRemove);
	}

	// Remove any comment nodes which have nodes we don't contain
	TSet<UObject*> NodesToRemove;
	for (UObject* Obj : CommentNode->GetNodesUnderComment())
	{
		if (UEdGraphNode_Comment* OtherComment = Cast<UEdGraphNode_Comment>(Obj))
		{
			const auto OtherMainNodes = TSet<UObject*>(OtherComment->GetNodesUnderComment().FilterByPredicate(IsMajorNode));
			for (UObject* OtherMain : OtherMainNodes)
			{
				// if we don't contain any node in the other node node, the comment should be removed
				if (!OurMainNodes.Contains(OtherMain))
				{
					if (!IsHeaderComment(OtherComment))
					{
						NodesToRemove.Add(OtherComment);
					}
					break;
				}
			}
		}
	}

	FASCUtils::RemoveNodesFromComment(CommentNode, NodesToRemove);

	// Do nothing if we have no nodes under ourselves
	if (CommentNode->GetNodesUnderComment().Num() == 0)
	{
		return;
	}

	bool bNeedsPurging = false;
	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : OtherCommentNodes)
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();

		if (OtherComment == CommentNode)
		{
			continue;
		}

		const auto OtherMainNodes = OtherComment->GetNodesUnderComment().FilterByPredicate(IsMajorNode);

		if (OtherMainNodes.Num() == 0)
		{
			continue;
		}

		// check if all nodes in the other comment box are within our comment box AND we are not inside the other comment already
		const bool bAllNodesContainedUnderSelf = !OtherMainNodes.ContainsByPredicate([&OurMainNodes](UObject* NodeUnderOther)
		{
			return !OurMainNodes.Contains(NodeUnderOther);
		});

		bool bDontAddSameSet;

		// Check if we should add the comment if the same main node set
		if (OldParentComments && OldCommentContains)
		{
			const bool bPreviouslyWasParent = OldParentComments->Contains(OtherComment);
			const bool bWeAreFreshNode = OldParentComments->Num() == 0 && OldCommentContains->Num() == 0;
			bDontAddSameSet = OurMainNodes.Num() == OtherMainNodes.Num() && (bPreviouslyWasParent || bWeAreFreshNode);
		}
		else
		{
			const bool bPreviouslyWasParent = CurrentParentComments.Contains(OtherComment);
			bDontAddSameSet = OurMainNodes.Num() == OtherMainNodes.Num() && bPreviouslyWasParent;
		}

		// we contain all of the other comment, add the other comment (unless we already contain it)
		if (bAllNodesContainedUnderSelf && !bDontAddSameSet)
		{
			// add the other comment into ourself
			FASCUtils::AddNodeIntoComment(CommentNode, OtherComment);
			bNeedsPurging = true;
		}
		else
		{
			// check if all nodes in the other comment box are within our comment box
			const bool bAllNodesContainedUnderOther = !OurMainNodes.ContainsByPredicate([&OtherMainNodes](UObject* NodeUnderSelf)
			{
				return !OtherMainNodes.Contains(NodeUnderSelf);
			});

			// other comment contains all of our nodes, add ourself into the other comment
			if (bAllNodesContainedUnderOther) 
			{
				// add the ourselves into the other comment
				FASCUtils::AddNodeIntoComment(OtherComment, CommentNode);
				bNeedsPurging = true;
			}
		}
	}

	if (bNeedsPurging && UAutoSizeCommentsSettings::Get().bEnableFixForSortDepthIssue)
	{
		FAutoSizeCommentGraphHandler::Get().RequestGraphVisualRefresh(GetOwnerPanel());
	}
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentBodyColor() const
{
	if (!CommentNode)
	{
		return FLinearColor::White;
	}

	return IsNodeUnrelated()
		? CommentNode->CommentColor * FLinearColor(0.5f, 0.5f, 0.5f, 0.4f)
		: CommentNode->CommentColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentTitleBarColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->CommentColor * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B, IsNodeUnrelated() ? 0.4f : 1.0f);
	}

	const FLinearColor Color = FLinearColor::White * 0.6f;
	return FLinearColor(Color.R, Color.G, Color.B);
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentBubbleColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->bColorCommentBubble
			? (CommentNode->CommentColor * 0.6f)
			: GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

		return FLinearColor(Color.R, Color.G, Color.B);
	}
	const FLinearColor Color = FLinearColor::White * 0.6f;
	return FLinearColor(Color.R, Color.G, Color.B);
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsColor() const
{
	return CommentControlsColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsTextColor() const
{
	return CommentControlsTextColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetPresetColor(const FLinearColor Color) const
{
	FLinearColor MyColor = Color;
	MyColor.A = OpacityValue;
	return MyColor;
}

float SAutoSizeCommentsGraphNode::GetWrapAt() const
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();
	const float HeaderSize = ASCSettings.bHideHeaderButton ? 0 : 20;
	const float AnchorPointWidth = ASCSettings.bHideCornerPoints ? 0 : 32;
	const float TextPadding = ASCSettings.CommentTextPadding.Left + ASCSettings.CommentTextPadding.Right;
	return FMath::Max(0.f, CachedWidth - AnchorPointWidth - HeaderSize - TextPadding - 12);
}

FASCCommentData& SAutoSizeCommentsGraphNode::GetCommentData() const
{
	return FAutoSizeCommentsCacheFile::Get().GetCommentData(CommentNode);
}

void SAutoSizeCommentsGraphNode::ResizeToFit()
{
	ResizeToFit_Impl();

	if (UAutoSizeCommentsSettings::Get().bUseTwoPassResize && GetResizingMode() == EASCResizingMode::Reactive)
	{
		TwoPassResizeDelay = 2;
	}
}

void SAutoSizeCommentsGraphNode::ResizeToFit_Impl()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::ResizeToFit"), STAT_ASC_ResizeToFit, STATGROUP_AutoSizeComments);

	// resize to fit the bounds of the nodes under the comment
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

		// get bounds and apply padding
		const FVector2D& Padding = ASCSettings.CommentNodePadding;

		const float VerticalPadding = FMath::Max(ASCSettings.MinimumVerticalPadding, Padding.Y); // ensure we can always see the buttons

		const float TopPadding = (!ASCSettings.bHidePresets || !ASCSettings.bHideRandomizeButton) ? VerticalPadding : Padding.Y;

		const float BottomPadding = !ASCSettings.bHideCommentBoxControls ? VerticalPadding : Padding.Y;

		const FSlateRect Bounds = GetBoundsForNodesInside().ExtendBy(FMargin(Padding.X, TopPadding, Padding.X, BottomPadding));

		const float TitleBarHeight = GetTitleBarHeight();

		// check if size has changed
		FASCVector2 CurrSize = Bounds.GetSize();
		CurrSize.Y += TitleBarHeight;

		if (!UserSize.Equals(CurrSize, .1f))
		{
			UserSize = CurrSize;
			GetNodeObj()->ResizeNode(CurrSize);
		}

		// check if location has changed
		FASCVector2 DesiredPos = Bounds.GetTopLeft();
		DesiredPos.Y -= TitleBarHeight;

		// move to desired pos
		if (!GetPos().Equals(DesiredPos, .1f))
		{
			GraphNode->NodePosX = DesiredPos.X;
			GraphNode->NodePosY = DesiredPos.Y;
		}
	}
	else
	{
		bool bEdited = false;
		if (UserSize.X < 125) // the comment has no nodes, resize to default
		{
			bEdited = true;
			UserSize.X = 225;
		}

		if (UserSize.Y < 80)
		{
			bEdited = true;
			UserSize.Y = 150;
		}

		if (bEdited)
		{
			GetNodeObj()->ResizeNode(UserSize);
		}
	}
}

void SAutoSizeCommentsGraphNode::ApplyHeaderStyle()
{
	switch (UAutoSizeCommentsSettings::Get().HeaderColorMethod)
	{
		case EASCDefaultCommentColorMethod::Random:
		{
			RandomizeColor();
			break;
		}
		case EASCDefaultCommentColorMethod::Default:
		{
			const FPresetCommentStyle& Style = UAutoSizeCommentsSettings::Get().HeaderStyle;
			CommentNode->CommentColor = Style.Color;
			CommentNode->FontSize = Style.FontSize;
			break;
		}
		default:
			break;
	}
}

void SAutoSizeCommentsGraphNode::ApplyPresetStyle(const FPresetCommentStyle& Style)
{
	CommentNode->CommentColor = Style.Color;
	CommentNode->FontSize = Style.FontSize;

	SetIsHeader(Style.bSetHeader, false);
}

void SAutoSizeCommentsGraphNode::OnTitleChanged(const FString& OldTitle, const FString& NewTitle)
{
	// apply the preset style if the title starts with the correct prefix
	bool bMatchesPreset = false;
	for (const auto& Elem : UAutoSizeCommentsSettings::Get().TaggedPresets)
	{
		if (NewTitle.StartsWith(Elem.Key))
		{
			ApplyPresetStyle(Elem.Value);
			bMatchesPreset = true;
		}
	}

	if (!bMatchesPreset)
	{
		// randomize our color if the old title was an auto applied preset
		bool bShouldResetColor = false;
		for (const auto& Elem : UAutoSizeCommentsSettings::Get().TaggedPresets)
		{
			if (OldTitle.StartsWith(Elem.Key))
			{
				bShouldResetColor = true;
				break;
			}
		}

		if (bShouldResetColor)
		{
			ApplyDefaultCommentColorMethod();
		}
	}
}

void SAutoSizeCommentsGraphNode::MoveEmptyCommentBoxes()
{
	TArray<UObject*> UnderComment = CommentNode->GetNodesUnderComment();

	TSharedPtr<SGraphPanel> OwnerPanel = OwnerGraphPanelPtr.Pin();

	bool bIsSelected = OwnerPanel->SelectionManager.IsNodeSelected(GraphNode);

	bool bIsContained = false;
	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : GetOtherCommentNodes())
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();

		if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
		{
			bIsContained = true;
			break;
		}
	}

	// if the comment node is empty, move away from other comment nodes
	if (UnderComment.Num() == 0 && UAutoSizeCommentsSettings::Get().bMoveEmptyCommentBoxes && !bIsSelected && !bIsContained && !IsHeaderComment())
	{
		FASCVector2 TotalMovement(0, 0);

		bool bAnyCollision = false;

		for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : GetOtherCommentNodes())
		{
			if (OtherCommentNode->IsHeaderComment())
			{
				continue;
			}

			UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();

			if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
			{
				continue;
			}

			FSlateRect OtherBounds = GetCommentBounds(OtherComment);
			FSlateRect MyBounds = GetCommentBounds(CommentNode);

			if (FSlateRect::DoRectanglesIntersect(OtherBounds, MyBounds))
			{
				float DeltaLeft = OtherBounds.Left - MyBounds.Right;
				float DeltaRight = OtherBounds.Right - MyBounds.Left;

				float DeltaTop = OtherBounds.Top - MyBounds.Bottom;
				float DeltaBottom = OtherBounds.Bottom - MyBounds.Top;

				float SelectedX = FMath::Abs(DeltaLeft) < FMath::Abs(DeltaRight) ? DeltaLeft : DeltaRight;
				float SelectedY = FMath::Abs(DeltaTop) < FMath::Abs(DeltaBottom) ? DeltaTop : DeltaBottom;

				if (FMath::Abs(SelectedX) < FMath::Abs(SelectedY))
				{
					TotalMovement.X += FMath::Sign(SelectedX);
				}
				else
				{
					TotalMovement.Y += FMath::Sign(SelectedY);
				}

				bAnyCollision = true;
			}
		}

		if (TotalMovement.SizeSquared() == 0 && bAnyCollision)
		{
			TotalMovement.X = (FMath::Rand() % 2) * 2 - 1;
			TotalMovement.Y = (FMath::Rand() % 2) * 2 - 1;
		}

		TotalMovement *= UAutoSizeCommentsSettings::Get().EmptyCommentBoxSpeed;

		GraphNode->NodePosX += TotalMovement.X;
		GraphNode->NodePosY += TotalMovement.Y;
	}
}

void SAutoSizeCommentsGraphNode::CreateCommentControls()
{
	// Create the replace button
	TSharedRef<SButton> ReplaceButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Replace with selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("R")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	// Create the add button
	TSharedRef<SButton> AddButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleAddButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Add selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				TSharedRef<SWidget>(
					SNew(SImage)
					.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					.Image(FCoreStyle::Get().GetBrush("EditableComboBox.Add")
					))
			]
		];

	// Create the remove button
	TSharedRef<SButton> RemoveButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleSubtractButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Remove selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				TSharedRef<SWidget>(
					SNew(SImage)
					.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					.Image(FCoreStyle::Get().GetBrush("EditableComboBox.Delete")
					))
			]
		];

	// Create the clear button
	TSharedRef<SButton> ClearButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleClearButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Clear all nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("C")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	// Create the comment controls
	CommentControls = SNew(SHorizontalBox);
	CommentControls->AddSlot().AttachWidget(ReplaceButton);
	CommentControls->AddSlot().AttachWidget(AddButton);
	CommentControls->AddSlot().AttachWidget(RemoveButton);
	CommentControls->AddSlot().AttachWidget(ClearButton);
}

void SAutoSizeCommentsGraphNode::CreateColorControls()
{
	// Create the color controls
	ColorControls = SNew(SHorizontalBox);

	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	const TArray<FPresetCommentStyle>& Presets = ASCSettings.PresetStyles;
	CachedNumPresets = Presets.Num();

	if (!IsHeaderComment()) // header comments don't need color presets
	{
		if (!ASCSettings.bHideResizeButton && GetResizingMode() != EASCResizingMode::Always)
		{
			// Create the resize button
			ResizeButton = SNew(SButton)
				.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
				.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
				.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleResizeButtonClicked)
				.ContentPadding(FMargin(2, 2))
				.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
				.ToolTipText(FText::FromString("Resize to containing nodes"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(12).HeightOverride(12)
					[
						SNew(SImage)
						.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
						.Image(ASC_STYLE_CLASS::Get().GetBrush("Icons.Refresh"))
					]
				];

			ColorControls->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f).AttachWidget(ResizeButton.ToSharedRef());
		}

		ColorControls->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

		auto Buttons = SNew(SHorizontalBox);
		ColorControls->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Fill).AttachWidget(Buttons);

		if (!ASCSettings.bHidePresets)
		{
			for (const FPresetCommentStyle& Preset : Presets)
			{
				FLinearColor ColorWithoutOpacity = Preset.Color;
				ColorWithoutOpacity.A = 1;

				TSharedRef<SButton> Button = SNew(SButton)
					.ButtonStyle(ASC_STYLE_CLASS::Get(), "RoundButton")
					.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetPresetColor, ColorWithoutOpacity)
					.OnClicked(this, &SAutoSizeCommentsGraphNode::HandlePresetButtonClicked, Preset)
					.ContentPadding(FMargin(2, 2))
					.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
					.ToolTipText(FText::FromString("Set preset color"))
					[
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					];

				Buttons->AddSlot().AttachWidget(Button);
			}
		}

		if (!ASCSettings.bHideRandomizeButton)
		{
			// Create the random color button
			TSharedRef<SButton> RandomColorButton = SNew(SButton)
				.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
				.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
				.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked)
				.ContentPadding(FMargin(2, 2))
				.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
				.ToolTipText(FText::FromString("Randomize the color of the comment box"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString("?")))
						.Font(ASC_GET_FONT_STYLE("BoldFont"))
						.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					]
				];

			Buttons->AddSlot().AttachWidget(RandomColorButton);
		}
	}
}

/***************************
****** Util functions ******
****************************/

TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> SAutoSizeCommentsGraphNode::GetOtherCommentNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel || !OwnerPanel.IsValid())
	{
		return TSet<TSharedPtr<SAutoSizeCommentsGraphNode>>();
	}

	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Get list of all other comment nodes
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> OtherCommentNodes;
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		TSharedPtr<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = StaticCastSharedPtr<SAutoSizeCommentsGraphNode>(SomeNodeWidget);
		if (ASCGraphNode.IsValid())
		{
			UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();
			if (UEdGraphNode_Comment* GraphCommentNode = Cast<UEdGraphNode_Comment>(GraphObject))
			{
				if (GraphCommentNode != CommentNode)
				{
					OtherCommentNodes.Add(ASCGraphNode);
				}
			}
		}
	}

	return OtherCommentNodes;
}

TArray<UEdGraphNode_Comment*> SAutoSizeCommentsGraphNode::GetParentComments() const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::GetParentComments"), STAT_ASC_GetParentComments, STATGROUP_AutoSizeComments);
	TArray<UEdGraphNode_Comment*> ParentComments;

	for (UEdGraphNode* OtherNode : CommentNode->GetGraph()->Nodes)
	{
		if (auto OtherComment = Cast<UEdGraphNode_Comment>(OtherNode))
		{
			if (OtherComment != CommentNode && OtherComment->GetNodesUnderComment().Contains(CommentNode))
			{
				ParentComments.Add(OtherComment);
			}
		}
	}

	return ParentComments;
}

FSlateRect SAutoSizeCommentsGraphNode::GetCommentBounds(UEdGraphNode_Comment* InCommentNode)
{
	FASCVector2 Point(InCommentNode->NodePosX, InCommentNode->NodePosY);
	FASCVector2 Extent(InCommentNode->NodeWidth, InCommentNode->NodeHeight);
	return FSlateRect::FromPointAndExtent(Point, Extent);
}

void SAutoSizeCommentsGraphNode::SnapVectorToGrid(FASCVector2& Vector)
{
	const float SnapSize = SNodePanel::GetSnapGridSize();
	Vector.X = SnapSize * FMath::RoundToFloat(Vector.X / SnapSize);
	Vector.Y = SnapSize * FMath::RoundToFloat(Vector.Y / SnapSize);
}

void SAutoSizeCommentsGraphNode::SnapBoundsToGrid(FSlateRect& Bounds, int GridMultiplier)
{
	const float SnapSize = SNodePanel::GetSnapGridSize() * GridMultiplier;
	Bounds.Left = SnapSize * FMath::FloorToInt(Bounds.Left / SnapSize);
	Bounds.Right = SnapSize * FMath::CeilToInt(Bounds.Right / SnapSize);
	Bounds.Top = SnapSize * FMath::FloorToInt(Bounds.Top / SnapSize);
	Bounds.Bottom = SnapSize * FMath::CeilToInt(Bounds.Bottom / SnapSize);
}

bool SAutoSizeCommentsGraphNode::IsLocalPositionInCorner(const FASCVector2& MousePositionInNode) const
{
	FASCVector2 CornerBounds = GetDesiredSize() - FASCVector2(40, 40);
	return MousePositionInNode.Y >= CornerBounds.Y && MousePositionInNode.X >= CornerBounds.X;
}

EASCAnchorPoint SAutoSizeCommentsGraphNode::GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	const FASCVector2 MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	const FASCVector2 Size = GetDesiredSize();

	// check the mouse position is actually in the node
	if (MousePositionInNode.X < 0 || MousePositionInNode.X > Size.X ||
		MousePositionInNode.Y < 0 || MousePositionInNode.Y > Size.Y)
	{
		return EASCAnchorPoint::None;
	}

	// scale the anchor size by the graph zoom
	float AnchorSizeScale = 1.0f;
	if (OwnerGraphPanelPtr.IsValid())
	{
		const float GraphZoom = OwnerGraphPanelPtr.Pin()->GetZoomAmount();
		if (GraphZoom > 0.0f)
		{
			AnchorSizeScale = FMath::Clamp(1.0f / GraphZoom, 1.0f, 4.0f);
		}
	}

	const float CornerAnchorSize = UAutoSizeCommentsSettings::Get().ResizeCornerAnchorSize * AnchorSizeScale;
	const float SideAnchorSize = UAutoSizeCommentsSettings::Get().ResizeSidePadding * AnchorSizeScale;

	const float SidePadding = SideAnchorSize;
	const float Top = CornerAnchorSize;
	const float Left = CornerAnchorSize;
	const float Right = Size.X - CornerAnchorSize;
	const float Bottom = Size.Y - CornerAnchorSize;

	if (!IsHeaderComment()) // header comments should only have anchor points to resize left and right
	{
		if (MousePositionInNode.X > Right && MousePositionInNode.Y > Bottom)
		{
			return EASCAnchorPoint::BottomRight;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y < Top)
		{
			return EASCAnchorPoint::TopLeft;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y > Bottom)
		{
			return EASCAnchorPoint::BottomLeft;
		}
		if (MousePositionInNode.X > Right && MousePositionInNode.Y < Top)
		{
			return EASCAnchorPoint::TopRight;
		}
		if (MousePositionInNode.Y < SidePadding)
		{
			return EASCAnchorPoint::Top;
		}
		if (MousePositionInNode.Y > Size.Y - SidePadding)
		{
			return EASCAnchorPoint::Bottom;
		}
	}
	if (MousePositionInNode.X < SidePadding)
	{
		return EASCAnchorPoint::Left;
	}
	if (MousePositionInNode.X > Size.X - SidePadding)
	{
		return EASCAnchorPoint::Right;
	}

	return EASCAnchorPoint::None;
}

void SAutoSizeCommentsGraphNode::SetIsHeader(bool bNewValue, bool bUpdateStyle)
{
	// do nothing if we are already a header
	if (bIsHeader == bNewValue)
	{
		return;
	}

	bIsHeader = bNewValue;

	// update the comment data
	FASCCommentData& CommentData = GetCommentData();
	CommentData.SetHeader(bNewValue);

	if (bUpdateStyle)
	{
		if (bIsHeader) // apply header style
		{
			ApplyHeaderStyle();
			FASCUtils::ClearCommentNodes(CommentNode);
		}
		else // undo header style
		{
			// only refresh the color if the color matches the header style color 
			if (CommentNode->CommentColor == UAutoSizeCommentsSettings::Get().HeaderStyle.Color)
			{
				if (UAutoSizeCommentsSettings::Get().DefaultCommentColorMethod == EASCDefaultCommentColorMethod::Random)
				{
					RandomizeColor();
				}
				else
				{
					CommentNode->CommentColor = UAutoSizeCommentsSettings::Get().DefaultCommentColor;
				}
			}

			CommentNode->FontSize = UAutoSizeCommentsSettings::Get().DefaultFontSize;
			AdjustMinSize(UserSize);
			CommentNode->ResizeNode(UserSize);
		}
	}

	UpdateGraphNode();
}

bool SAutoSizeCommentsGraphNode::IsHeaderComment() const
{
	return bIsHeader;
}

bool SAutoSizeCommentsGraphNode::IsHeaderComment(UEdGraphNode_Comment* OtherComment)
{
	return FAutoSizeCommentsCacheFile::Get().GetCommentData(OtherComment).IsHeader();
}

FKey SAutoSizeCommentsGraphNode::GetResizeKey() const
{
	return UAutoSizeCommentsSettings::Get().ResizeChord.Key;
}

bool SAutoSizeCommentsGraphNode::AreResizeModifiersDown(bool bDownIfNoModifiers) const
{
	const FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	const FInputChord ResizeChord = UAutoSizeCommentsSettings::Get().ResizeChord;

	if (!ResizeChord.HasAnyModifierKeys())
	{
		return bDownIfNoModifiers;
	}

	return KeysState.AreModifersDown(EModifierKey::FromBools(ResizeChord.bCtrl, ResizeChord.bAlt, ResizeChord.bShift, ResizeChord.bCmd));
}

bool SAutoSizeCommentsGraphNode::IsSingleSelectedNode() const
{
	TSharedPtr<SGraphPanel> OwnerPanel = OwnerGraphPanelPtr.Pin();
	return OwnerPanel->SelectionManager.GetSelectedNodes().Num() == 1 && OwnerPanel->SelectionManager.IsNodeSelected(GraphNode); 
}

bool SAutoSizeCommentsGraphNode::IsNodeUnrelated() const
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	return CommentNode->IsNodeUnrelated();
#else
	return false;
#endif
}

void SAutoSizeCommentsGraphNode::SetNodesRelated(const TArray<UEdGraphNode*>& Nodes, bool bIncludeSelf)
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	const TArray<UEdGraphNode*>& AllNodes = GetNodeObj()->GetGraph()->Nodes;
	for (UEdGraphNode* Node : AllNodes)
	{
		Node->SetNodeUnrelated(true);
	}

	if (bIncludeSelf)
	{
		GetCommentNodeObj()->SetNodeUnrelated(false);
	}

	for (UEdGraphNode* Node : Nodes)
	{
		Node->SetNodeUnrelated(false);
	}
#endif
}

void SAutoSizeCommentsGraphNode::ResetNodesUnrelated()
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	if (UEdGraph* Graph = GetNodeObj()->GetGraph())
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			Node->SetNodeUnrelated(false);
		}
	}
#endif
}

bool SAutoSizeCommentsGraphNode::IsExistingComment() const
{
	if (CommentNode)
	{
		FASCGraphHandlerData& GraphData = FAutoSizeCommentGraphHandler::Get().GetGraphHandlerData(CommentNode->GetGraph());
		return GraphData.InitialComments.Contains(CommentNode);
	}

	return false;
}

EASCResizingMode SAutoSizeCommentsGraphNode::GetResizingMode() const
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();
	if (const FASCGraphSettings* GraphSettings = ASCSettings.GraphSettingsOverride.Find(CachedGraphClassName))
	{
		return GraphSettings->ResizingMode;
	}

	return ASCSettings.ResizingMode;
}

FASCCommentData& SAutoSizeCommentsGraphNode::GetCommentData()
{
	return FAutoSizeCommentsCacheFile::Get().GetCommentData(CommentNode);
}

EGraphRenderingLOD::Type SAutoSizeCommentsGraphNode::GetLOD() const
{
	return FAutoSizeCommentGraphHandler::Get().GetGraphLOD(OwnerGraphPanelPtr.Pin());
}

bool SAutoSizeCommentsGraphNode::AreControlsEnabled() const
{
	return bAreControlsEnabled;
}

bool SAutoSizeCommentsGraphNode::IsPresetStyle()
{
	for (const FPresetCommentStyle& Style : UAutoSizeCommentsSettings::Get().PresetStyles)
	{
		if (CommentNode->CommentColor == Style.Color && CommentNode->FontSize == Style.FontSize)
		{
			return true;
		}
	}

	return false;
}

bool SAutoSizeCommentsGraphNode::LoadCache()
{
	CommentNode->ClearNodesUnderComment();

	TArray<UEdGraphNode*> OutNodesUnder;
	if (FAutoSizeCommentsCacheFile::Get().GetNodesUnderComment(SharedThis(this), OutNodesUnder))
	{
		for (UEdGraphNode* Node : OutNodesUnder)
		{
			if (!HasNodeBeenDeleted(Node))
			{
				FASCUtils::AddNodeIntoComment(CommentNode, Node, false);
			}
		}

		return true;
	}

	return false;
}

void SAutoSizeCommentsGraphNode::UpdateCache()
{
	GetCommentData().UpdateNodesUnderComment(CommentNode);
}

void SAutoSizeCommentsGraphNode::QueryNodesUnderComment(TArray<UEdGraphNode*>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots)
{
	TArray<TSharedPtr<SGraphNode>> OutGraphNodes;
	QueryNodesUnderComment(OutGraphNodes, OverrideCollisionMethod, bIgnoreKnots);
	for (TSharedPtr<SGraphNode>& Node : OutGraphNodes)
	{
		OutNodesUnderComment.Add(Node->GetNodeObj());
	}
}

void SAutoSizeCommentsGraphNode::QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots)
{
	if (OverrideCollisionMethod == ECommentCollisionMethod::Disabled)
	{
		return;
	}

	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	float TitleBarHeight = GetTitleBarHeight();

	const FASCVector2 NodeSize(UserSize.X, UserSize.Y - TitleBarHeight);

	// Get our geometry
	FASCVector2 NodePosition = GetPos();
	NodePosition.Y += TitleBarHeight;

	const FSlateRect CommentRect = FSlateRect::FromPointAndExtent(NodePosition, NodeSize).ExtendBy(1);

	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Iterate across all nodes in the graph
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		const TSharedRef<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));

		UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();
		if (GraphObject == nullptr || GraphObject == CommentNode)
		{
			continue;
		}

		// check if the node bounds collides with our bounds
		const FASCVector2 SomeNodePosition = FASCUtils::GetNodePos(&SomeNodeWidget.Get());
		const FASCVector2 SomeNodeSize = SomeNodeWidget->GetDesiredSize();
		const FSlateRect NodeGeometryGraphSpace = FSlateRect::FromPointAndExtent(SomeNodePosition, SomeNodeSize);

		bool bIsOverlapping = false;

		switch (OverrideCollisionMethod)
		{
			case ECommentCollisionMethod::Point:
				bIsOverlapping = CommentRect.ContainsPoint(SomeNodePosition);
				break;
			case ECommentCollisionMethod::Intersect:
				CommentRect.IntersectionWith(NodeGeometryGraphSpace, bIsOverlapping);
				break;
			case ECommentCollisionMethod::Contained:
				bIsOverlapping = FSlateRect::IsRectangleContained(CommentRect, NodeGeometryGraphSpace);
				break;
			default: ;
		}

		if (bIsOverlapping)
		{
			OutNodesUnderComment.Add(SomeNodeWidget);
		}
	}
}

void SAutoSizeCommentsGraphNode::RandomizeColor()
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	if (ASCSettings.bUseRandomColorFromList)
	{
		const int RandIndex = FMath::Rand() % ASCSettings.PredefinedRandomColorList.Num();
		if (ASCSettings.PredefinedRandomColorList.IsValidIndex(RandIndex))
		{
			CommentNode->CommentColor = ASCSettings.PredefinedRandomColorList[RandIndex];
		}
	}
	else
	{
		CommentNode->CommentColor = FLinearColor::MakeRandomColor();
		CommentNode->CommentColor.A = ASCSettings.RandomColorOpacity;
	}
}

void SAutoSizeCommentsGraphNode::AdjustMinSize(FASCVector2& InSize)
{
	const float MinY = IsHeaderComment() ? 0 : 80;
	InSize = FASCVector2(FMath::Max(125.f, InSize.X), FMath::Max(GetTitleBarHeight() + MinY, InSize.Y));
}

bool SAutoSizeCommentsGraphNode::HasNodeBeenDeleted(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return true;
	}

	return !CommentNode->GetGraph()->Nodes.Contains(Node);
}

bool SAutoSizeCommentsGraphNode::CanAddNode(const TSharedPtr<SGraphNode> OtherGraphNode, const bool bIgnoreKnots) const
{
	UObject* GraphObject = OtherGraphNode->GetObjectBeingDisplayed();
	if (GraphObject == nullptr || GraphObject == CommentNode)
	{
		return false;
	}

	if ((bIgnoreKnots || UAutoSizeCommentsSettings::Get().bIgnoreKnotNodes) && Cast<UK2Node_Knot>(GraphObject) != nullptr)
	{
		return false;
	}

	if (Cast<UEdGraphNode_Comment>(GraphObject))
	{
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode = StaticCastSharedPtr<SAutoSizeCommentsGraphNode>(OtherGraphNode);
		if (!ASCNode.IsValid() || !ASCNode->IsHeaderComment())
		{
			return false;
		}
	}

	return true;
}

bool SAutoSizeCommentsGraphNode::CanAddNode(const UObject* Node, const bool bIgnoreKnots) const
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel.IsValid())
	{
		return false;
	}

	const UEdGraphNode* EdGraphNode = Cast<UEdGraphNode>(Node);
	if (!EdGraphNode)
	{
		return false;
	}

	TSharedPtr<SGraphNode> NodeAsGraphNode = OwnerPanel->GetNodeWidgetFromGuid(EdGraphNode->NodeGuid);
	if (!NodeAsGraphNode.IsValid())
	{
		return false;
	}

	return CanAddNode(NodeAsGraphNode, bIgnoreKnots);
}

void SAutoSizeCommentsGraphNode::OnAltReleased()
{
	FAutoSizeCommentGraphHandler::Get().ProcessAltReleased(GetOwnerPanel());

	if (!UAutoSizeCommentsSettings::Get().bHighlightContainingNodesOnSelection)
	{
		ResetNodesUnrelated();
	}
}

bool SAutoSizeCommentsGraphNode::IsCommentNode(UObject* Object)
{
	return Object->IsA(UEdGraphNode_Comment::StaticClass());
}

FSlateRect SAutoSizeCommentsGraphNode::GetNodeBounds(UEdGraphNode* Node)
{
	if (!Node)
	{
		return FSlateRect();
	}

	FASCVector2 Pos(Node->NodePosX, Node->NodePosY);
	FASCVector2 Size(300, 150);

	TSharedPtr<SGraphNode> LocalGraphNode = FASCUtils::GetGraphNode(GetOwnerPanel(), Node);
	if (LocalGraphNode.IsValid())
	{
		Pos = FASCUtils::GetNodePos(LocalGraphNode.Get());
		Size = LocalGraphNode->GetDesiredSize();

		if (UAutoSizeCommentsSettings::Get().bUseCommentBubbleBounds && Node->bCommentBubbleVisible)
		{
			if (FNodeSlot* CommentSlot = LocalGraphNode->GetSlot(ENodeZone::TopCenter))
			{
				TSharedPtr<SCommentBubble> LocalCommentBubble = StaticCastSharedRef<SCommentBubble>(CommentSlot->GetWidget());

				if (LocalCommentBubble.IsValid() && LocalCommentBubble->IsBubbleVisible())
				{
					FASCVector2 CommentBubbleSize = LocalCommentBubble->GetDesiredSize();
					Pos.Y -= CommentBubbleSize.Y;
					Size.Y += CommentBubbleSize.Y;
					Size.X = FMath::Max(Size.X, CommentBubbleSize.X);
				}
			}
		}
	}

	return FSlateRect::FromPointAndExtent(Pos, Size);
}

bool SAutoSizeCommentsGraphNode::AnySelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	return OwnerPanel->SelectionManager.GetSelectedNodes().Num() > 0;
}

FSlateRect SAutoSizeCommentsGraphNode::GetBoundsForNodesInside()
{
	TArray<UEdGraphNode*> Nodes;
	for (UObject* Obj : CommentNode->GetNodesUnderComment())
	{
		if (UEdGraphNode_Comment* OtherCommentNode = Cast<UEdGraphNode_Comment>(Obj))
		{
			// if the node contains us and is higher depth, do not resize
			if (OtherCommentNode->GetNodesUnderComment().Contains(GraphNode) &&
				OtherCommentNode->CommentDepth > CommentNode->CommentDepth)
			{
				continue;
			}
		}

		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			Nodes.Add(Node);
		}
	}

	bool bBoundsInit = false;
	FSlateRect Bounds;
	for (int i = 0; i < Nodes.Num(); i++)
	{
		UEdGraphNode* Node = Nodes[i];

		if (!Node)
		{
			continue;
		}

		// initialize bounds from first valid node
		if (!bBoundsInit)
		{
			Bounds = GetNodeBounds(Node);
			bBoundsInit = true;
		}
		else
		{
			Bounds = Bounds.Expand(GetNodeBounds(Node));
		}
	}

	return Bounds;
}

TArray<UEdGraphNode*> SAutoSizeCommentsGraphNode::GetNodesUnderComment() const
{
	return FASCUtils::GetNodesUnderComment(CommentNode);
}

bool SAutoSizeCommentsGraphNode::IsMajorNode(UObject* Object)
{
	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Object))
	{
		if (IsHeaderComment(CommentNode))
		{
			return true;
		}
	}
	else if (Object->IsA(UEdGraphNode::StaticClass()))
	{
		return true;
	}

	return false;
}