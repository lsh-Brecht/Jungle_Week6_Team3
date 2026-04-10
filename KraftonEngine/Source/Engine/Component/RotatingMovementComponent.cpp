#include "RotatingMovementComponent.h"

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

  const FRotator DeltaRotation = RotationRate * DeltaTime;
	if (DeltaRotation.IsNearlyZero())
	{
		return;
	}

	const FQuat DeltaQuat = DeltaRotation.ToQuaternion();
	const FVector OldWorldLocation = UpdatedSceneComponent->GetWorldLocation();
	const FQuat OldWorldQuat = UpdatedSceneComponent->GetWorldMatrix().ToQuat();

	FQuat NewWorldQuat = OldWorldQuat;

	if (bRotationInlocalSpace)
	{
		// 로컬 축 기준 회전
		UpdatedSceneComponent->AddLocalRotation(DeltaQuat);
		NewWorldQuat = UpdatedSceneComponent->GetWorldMatrix().ToQuat();
	}
	else
	{
		// 월드 축 기준 회전
		NewWorldQuat = (DeltaQuat * OldWorldQuat).GetNormalized();

		USceneComponent* ParentComponent = UpdatedSceneComponent->GetParent();
		if (ParentComponent)
		{
			const FQuat ParentWorldQuat = ParentComponent->GetWorldMatrix().ToQuat();
			const FQuat NewRelativeQuat = (NewWorldQuat * ParentWorldQuat.Inverse()).GetNormalized();
			UpdatedSceneComponent->SetRelativeRotation(NewRelativeQuat);
		}
		else
		{
			UpdatedSceneComponent->SetRelativeRotation(NewWorldQuat);
		}
	}

	if (PivotTranslation.Length() > 0.0f)
	{
		const FVector OldPivotOffsetWorld = OldWorldQuat.RotateVector(PivotTranslation);
		const FVector NewPivotOffsetWorld = NewWorldQuat.RotateVector(PivotTranslation);
		const FVector PivotWorldLocation = OldWorldLocation - OldPivotOffsetWorld;
		const FVector NewWorldLocation = PivotWorldLocation + NewPivotOffsetWorld;
		UpdatedSceneComponent->SetWorldLocation(NewWorldLocation);
	}
}

void URotatingMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << RotationRate.Pitch;
	Ar << RotationRate.Yaw;
	Ar << RotationRate.Roll;
   Ar << bRotationInlocalSpace;
	Ar << PivotTranslation;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
   OutProps.push_back({ "Rotation In Local Space", EPropertyType::Bool, &bRotationInlocalSpace, 0.0f, 0.0f, 0.0f });
	OutProps.push_back({ "Pivot Translation", EPropertyType::Vec3, &PivotTranslation, 0.0f, 0.0f, 0.1f });
}
