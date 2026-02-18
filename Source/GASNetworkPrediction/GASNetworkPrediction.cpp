// Copyright Epic Games, Inc. All Rights Reserved.

#include "GASNetworkPrediction.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// GNP.DebugMode: 서버/클라이언트 자동 판별 후 적절한 디버그 설정 적용
static FAutoConsoleCommandWithWorldAndArgs GNPDebugModeCmd(
	TEXT("GNP.DebugMode"),
	TEXT("0: Off, 1: On - 서버/클라이언트 자동 판별 후 디버그 설정 적용"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() == 0) return;

		const int32 Mode = FCString::Atoi(*Args[0]);
		const bool bIsServer = (World->GetNetMode() != NM_Client);

		if (Mode == 0)
		{
			// 끄기
			IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GNP.ShowHitscanDebug 0"), *GLog, World);
			IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GNP.ShowRewindDebug 0"), *GLog, World);
			if (!bIsServer)
			{
				GEngine->Exec(World, TEXT("Net PktLag=0"));
			}
			UE_LOG(LogTemp, Log, TEXT("[GNP] DebugMode OFF (%s)"), bIsServer ? TEXT("Server") : TEXT("Client"));
		}
		else
		{
			if (bIsServer)
			{
				// 서버: Rewind 캡슐 + 히트스캔 상세
				IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GNP.ShowHitscanDebug 2"), *GLog, World);
				IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GNP.ShowRewindDebug 1"), *GLog, World);
				UE_LOG(LogTemp, Log, TEXT("[GNP] DebugMode ON (Server): HitscanDebug=2, RewindDebug=1"));
			}
			else
			{
				// 클라이언트: 트레이스 라인 + 지연 시뮬레이션
				IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GNP.ShowHitscanDebug 1"), *GLog, World);
				GEngine->Exec(World, TEXT("Net PktLag=300"));
				UE_LOG(LogTemp, Log, TEXT("[GNP] DebugMode ON (Client): HitscanDebug=1, PktLag=300"));
			}
		}
	})
);

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, GASNetworkPrediction, "GASNetworkPrediction" );
