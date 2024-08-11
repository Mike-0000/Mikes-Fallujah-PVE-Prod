modded class SCR_AmbientPatrolSystem : GameSystem
{
	protected SCR_AIGroup spawnedGroup;	// Added by Gramps
	
	override protected void ProcessSpawnpoint(int spawnpointIndex)
	{
		SCR_AmbientPatrolSpawnPointComponent spawnpoint = m_aPatrols[spawnpointIndex];

		if (!spawnpoint || (spawnpoint.GetMembersAlive() == 0 && !spawnpoint.GetIsSpawned()))
			return;

		ChimeraWorld world = GetWorld();
		WorldTimestamp currentTime = world.GetServerTimestamp();
		if (spawnpoint.GetRespawnTimestamp().Greater(currentTime))
			return;

/* Gramps added - If vehicle group, TP to parent vehicle from vehicle spawnpoint >> */				
		if(spawnpoint.IsVehicleGroup())
		{
			if(!spawnpoint.GetIsSpawned())
			{
				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
				GetGame().GetCallqueue().CallLater(TeleportToVehicle,4000,false,spawnpoint);
			}
			return;
		}
/* << Gramps added */		

		bool playersNear;
		bool playersVeryNear;
		bool playersFar = true;
		vector location = spawnpoint.GetOwner().GetOrigin();
		int distance;
		m_iSpawnDistanceSq = 185000;
		m_iDespawnDistanceSq = 335000;
		

		// Define if any player is close enough to spawn or if all players are far enough to despawn
		foreach (IEntity player : m_aPlayers)
		{
			if (!player)
				continue;

			distance = vector.DistanceSq(player.GetOrigin(), location);

			if (distance > m_iDespawnDistanceSq)
				continue;

			playersFar = false;

			if (distance > m_iSpawnDistanceSq)
				continue;

			playersNear = true;

			if (distance > SPAWN_RADIUS_BLOCK_SQ)
				continue;

			playersVeryNear = true;
			break;
		}

		bool isAIOverLimit;
		AIWorld aiWorld = GetGame().GetAIWorld();

		if (aiWorld)
		{
			int maxChars = aiWorld.GetLimitOfActiveAIs();

			if (maxChars <= 0)
				isAIOverLimit = true;
			else
				isAIOverLimit = ((float)aiWorld.GetCurrentNumOfActiveAIs() / (float)maxChars) > spawnpoint.GetAILimitThreshold();
		}

		if (playersNear && !spawnpoint.GetIsSpawned() && !playersVeryNear){
			spawnpoint.SpawnPatrol();
			spawnpoint.SetIsPaused(false);
		}
		
		if (playersNear && !spawnpoint.GetIsPaused() && !spawnpoint.IsGroupActive())
		{
			// Do not spawn the patrol if the AI threshold setting has been reached
			if (isAIOverLimit)
			{
				spawnpoint.SetIsPaused(true);	// Make sure a patrol is not spawned too close to players when AI limit suddenly allows spawning of this group
				return;
			}

			spawnpoint.ActivateGroup();
			return;
		}
		
		// Gramps added this check to prevent ai capturing a point from despawning when players leave the area of a point under attack
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		SCR_CampaignMilitaryBaseComponent nearestBase;
		bool capturing = false;
		if (spawnpoint.GetIsSpawned() && spawnpoint.IsGroupActive())
		{
			SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
			if(group)
			{
				nearestBase = baseManager.FindClosestBase(group.GetOrigin());
				if(nearestBase.IsBeingCaptured())
				{
					vector basePos = nearestBase.GetOwner().GetOrigin();
					int dist = vector.Distance(group.GetOrigin(), basePos);
					if(dist < nearestBase.GetRadius())	capturing = true;	// Only despawn ai groups outside of base radius
				}
			}
		}
		// End Gramps edit
		
		// Delay is used so dying players don't see the despawn happen
		if (spawnpoint.GetIsSpawned() && playersFar && spawnpoint.IsGroupActive() /*Gramps added>>*/&& !spawnpoint.IsVehicleGroup() && !capturing)
		{
			WorldTimestamp despawnT = spawnpoint.GetDespawnTimer();

			if (despawnT == 0)
				spawnpoint.SetDespawnTimer(currentTime.PlusMilliseconds(DESPAWN_TIMEOUT));
			else if (currentTime.Greater(despawnT)){
				spawnpoint.DeactivateGroup();
				spawnpoint.DespawnPatrol();
			}
		}
		else
		{
			spawnpoint.SetDespawnTimer(null);
		}
	}

/* Gramps added - TP to parent vehicle from vehicle spawnpoint > */		
	void TeleportToVehicle(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		group.SetIsVehiclePatrol(spawnpoint.IsVehicleGroup());
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if(agents)
		{
			IEntity aiParent = spawnpoint.GetOwner().GetParent();
			SCR_AmbientVehicleSpawnPointComponent AVSPC;
			if(aiParent)
				AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
			Vehicle spawnedVehicle;
			if(AVSPC)
			{
				spawnedVehicle = AVSPC.GetSpawnedVehicle();
				if(spawnedVehicle)
				{
					array<string> groupVehicles = {};
					string vehicle = spawnedVehicle.ToString();
					groupVehicles.Insert(vehicle);
					group.SetGroupVehicles(groupVehicles);
					
					BaseCompartmentManagerComponent slotCompMan = BaseCompartmentManagerComponent.Cast(spawnedVehicle.FindComponent(BaseCompartmentManagerComponent));
					array<BaseCompartmentSlot> vehicleCompartments = new array<BaseCompartmentSlot>;
					slotCompMan.GetCompartments(vehicleCompartments);
					for(int j = 0; j < agents.Count(); j++)
					{
						AIAgent member = agents[j];
						if(member)
						{
							SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member.GetControlledEntity());
							CompartmentAccessComponent CAComp = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
							if(CAComp && character && !character.IsInVehicle())
							{
								BaseCompartmentSlot slot = vehicleCompartments[j];
								if(slot)
									CAComp.GetInVehicle(spawnedVehicle, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
							}
						}
					}
					
				}
			}
		}
	}
}
