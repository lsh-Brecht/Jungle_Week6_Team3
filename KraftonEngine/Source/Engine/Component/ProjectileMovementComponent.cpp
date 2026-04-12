#include "ProjectileMovementComponent.h"

#include "Collision/RayUtils.h"
#include "Component/FireBallComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SubUVComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Render/Pipeline/RenderBus.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

namespace
{
	/**
	 * 발사체의 속도를 시각적으로 표현하여 렌더 버스에 추가합니다.
	 * 
	 * \param RenderBus		렌더 버스에 디버그 라인을 추가합니다.
	 * \param Start			발사체의 시작 위치입니다.
	 * \param Velocity		발사체의 속도입니다.
	 */
	void AddProjectileVelocityArrow(FRenderBus& RenderBus, const FVector& Start, const FVector& Velocity)
	{
		constexpr float ProjectileArrowScale = 0.25f;
		const FVector ScaledVelocity = Velocity * ProjectileArrowScale;
		const float VelocityLength = ScaledVelocity.Length();
		if (VelocityLength <= FMath::Epsilon)
		{
			return;
		}

		const FVector Direction = ScaledVelocity / VelocityLength;
		const FVector End = Start + ScaledVelocity;
		const FColor ArrowColor(135, 206, 235);

		FDebugLineEntry Shaft;
		Shaft.Start = Start;
		Shaft.End = End;
		Shaft.Color = ArrowColor;
		RenderBus.AddDebugLineEntry(std::move(Shaft));

		const float HeadLength = Clamp(VelocityLength * 0.2f, 0.2f, 1.5f);
		FVector ReferenceUp(0.0f, 0.0f, 1.0f);
		if (std::abs(Direction.Dot(ReferenceUp)) > 0.98f)
		{
			ReferenceUp = FVector(0.0f, 1.0f, 0.0f);
		}

		const FVector Side = Direction.Cross(ReferenceUp).Normalized();
		const FVector Back = Direction * HeadLength;
		const FVector SideOffset = Side * (HeadLength * 0.45f);

		FDebugLineEntry HeadA;
		HeadA.Start = End;
		HeadA.End = End - Back + SideOffset;
		HeadA.Color = ArrowColor;
		RenderBus.AddDebugLineEntry(std::move(HeadA));

		FDebugLineEntry HeadB;
		HeadB.Start = End;
		HeadB.End = End - Back - SideOffset;
		HeadB.Color = ArrowColor;
		RenderBus.AddDebugLineEntry(std::move(HeadB));
	}

	UFireBallComponent* FindFireBallComponent(const AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UFireBallComponent* FireBallComponent = Cast<UFireBallComponent>(Component))
			{
				return FireBallComponent;
			}
		}

		return nullptr;
	}

	bool FindBlockingHit(UWorld* World, const AActor* IgnoredActor, const FRay& Ray, float MaxDistance, FHitResult& OutHitResult)
	{
		if (!World || MaxDistance <= FMath::Epsilon)
		{
			return false;
		}

		OutHitResult = {};
		OutHitResult.Distance = 3.402823466e+38F;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || Actor == IgnoredActor || !Actor->IsVisible())
			{
				continue;
			}

			for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
			{
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
				float BoxTMin = 0.0f;
				float BoxTMax = 0.0f;
				if (!FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, BoxTMin, BoxTMax))
				{
					continue;
				}

				if (BoxTMin > MaxDistance || BoxTMax < 0.0f)
				{
					continue;
				}

				FHitResult CandidateHit = {};
				if (!FRayUtils::RaycastComponent(Primitive, Ray, CandidateHit) || !CandidateHit.bHit)
				{
					continue;
				}

				if (CandidateHit.Distance < 0.0f || CandidateHit.Distance > MaxDistance)
				{
					continue;
				}

				if (CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
				}
			}
		}

		return OutHitResult.bHit;
	}

	void SpawnExplosionEffect(UWorld* World, const FVector& ImpactLocation, float SourceRadius)
	{
		if (!World)
		{
			return;
		}

		AActor* ExplosionActor = World->SpawnActor<AActor>();
		if (!ExplosionActor)
		{
			return;
		}

		USubUVComponent* ExplosionComponent = ExplosionActor->AddComponent<USubUVComponent>();
		if (!ExplosionComponent)
		{
			World->DestroyActor(ExplosionActor);
			return;
		}

		ExplosionActor->SetRootComponent(ExplosionComponent);
		ExplosionActor->SetActorLocation(ImpactLocation);

		const float ExplosionSize = std::max(SourceRadius * 6.0f, 12.0f);
		ExplosionComponent->SetVisibility(true);
		ExplosionComponent->SetParticle(FName("Explosion"));
		ExplosionComponent->SetSpriteSize(ExplosionSize, ExplosionSize);
		ExplosionComponent->SetFrameRate(30.0f);
		ExplosionComponent->SetLoop(false);
		ExplosionComponent->SetAutoDestroyOwnerWhenFinished(true);
		ExplosionComponent->Play();

		// SpawnActor 이후 컴포넌트를 추가한 경로라 BeginPlay는 수동으로 한 번 보장한다.
		if (ExplosionActor->HasActorBegunPlay())
		{
			ExplosionComponent->BeginPlay();
		}
	}
}

IMPLEMENT_CLASS(UProjectileMovementComponent, UMovementComponent)

void UProjectileMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();
}

void UProjectileMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	FVector EffectiveVelocity = ComputeEffectiveVelocity();
	const float CurrentSpeed = EffectiveVelocity.Length();
	if (MaxSpeed > 0.0f && CurrentSpeed > MaxSpeed)
	{
		EffectiveVelocity = EffectiveVelocity.Normalized() * MaxSpeed;
	}

	const FVector MoveDelta = EffectiveVelocity * DeltaTime;
	const float MoveDistance = MoveDelta.Length();
	if (MoveDistance <= FMath::Epsilon)
	{
		return;
	}

	const FVector CurrentLocation = UpdatedSceneComponent->GetWorldLocation();
	const FVector MoveDirection = MoveDelta / MoveDistance;
	const FRay MovementRay{ CurrentLocation, MoveDirection };

	if (UWorld* World = GetWorld())
	{
		FHitResult HitResult = {};
		if (FindBlockingHit(World, GetOwner(), MovementRay, MoveDistance, HitResult))
		{
			const float SafeMoveDistance = std::max(HitResult.Distance - 0.05f, 0.0f);
			UpdatedSceneComponent->SetWorldLocation(CurrentLocation + MoveDirection * SafeMoveDistance);
			if (HandleBlockingHit(UpdatedSceneComponent, CurrentLocation, MoveDelta, HitResult))
			{
				return;
			}
		}
	}

	UpdatedSceneComponent->SetWorldLocation(CurrentLocation + MoveDelta);
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, 0.0f, 10.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 0.0f, 10.0f });
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
}

void UProjectileMovementComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	const FVector PreviewVelocity = GetPreviewVelocity();
	if (PreviewVelocity.Length() <= FMath::Epsilon)
	{
		return;
	}

	USceneComponent* SourceComponent = GetUpdatedComponent();
	if (!SourceComponent)
	{
		AActor* OwnerActor = GetOwner();
		SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	}
	if (!SourceComponent)
	{
		return;
	}

	AddProjectileVelocityArrow(RenderBus, SourceComponent->GetWorldLocation(), PreviewVelocity);
}

void UProjectileMovementComponent::StopSimulating()
{
	Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UProjectileMovementComponent::GetPreviewVelocity() const
{
	return ComputeEffectiveVelocity();
}

EProjectileHitBehavior UProjectileMovementComponent::GetHitBehavior() const
{
	return EProjectileHitBehavior::Stop;
}

FVector UProjectileMovementComponent::ComputeEffectiveVelocity() const
{
	FVector EffectiveVelocity = Velocity;

	if (EffectiveVelocity.Length() <= FMath::Epsilon)
	{
		USceneComponent* SourceComponent = GetUpdatedComponent();
		if (!SourceComponent)
		{
			AActor* OwnerActor = GetOwner();
			SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
		}

		if (SourceComponent)
		{
			EffectiveVelocity = SourceComponent->GetForwardVector().Normalized();
		}
	}

	if (InitialSpeed > 0.0f && EffectiveVelocity.Length() > FMath::Epsilon)
	{
		EffectiveVelocity *= InitialSpeed;
	}

	return EffectiveVelocity;
}

bool UProjectileMovementComponent::HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult)
{
	(void)CurrentLocation;
	(void)MoveDelta;

	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!OwnerActor || !World)
	{
		return true;
	}

	if (UFireBallComponent* FireBallComponent = FindFireBallComponent(OwnerActor))
	{
		// FireBall이 다른 오브젝트에 닿으면 즉시 큰 폭발 SubUV를 한 번 재생시키고,
		// 발사체 actor는 제거한다.
		SpawnExplosionEffect(World, HitResult.WorldHitLocation, FireBallComponent->GetRadius());
		World->DestroyActor(OwnerActor);
		return true;
	}

	switch (GetHitBehavior())
	{
	case EProjectileHitBehavior::Destroy:
		World->DestroyActor(OwnerActor);
		return true;

	case EProjectileHitBehavior::Bounce:
		// 현재 Bounce는 별도 반사 계산을 지원하지 않으므로 정지 동작으로 폴백한다.
		[[fallthrough]];

	case EProjectileHitBehavior::Stop:
	default:
		if (UpdatedSceneComponent)
		{
			UpdatedSceneComponent->SetWorldLocation(HitResult.WorldHitLocation);
		}
		StopSimulating();
		return true;
	}
}
