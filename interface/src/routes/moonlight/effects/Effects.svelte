<svelte:options immutable={true} />

<script lang="ts">
	import { openModal, closeModal } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import { notifications } from '$lib/components/toasts/notifications';
	import DragDropList, { VerticalDropZone, reorder, type DropEvent } from 'svelte-dnd-list';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import EffectsIcon from '~icons/tabler/torii';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Delete from '~icons/tabler/trash';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import type { EffectsState } from '$lib/types/models';
	import Select from '$lib/components/Select.svelte';
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import type { StarState } from '$lib/types/models';

	let itemState: EffectsState;
	let starState: StarState;
	let itemsList: EffectsState[] = [];
	let editableItem: EffectsState = {
		name: '',
		effect: -1,
		projection: -1,
		nodes: []
	};

	let newItem: boolean = true;
	let showEditor: boolean = false;
	let formField: any;
	let formErrors = {
		name: false
	};
	let formErrorFilename = false;

	let dataLoaded = false;
	let starLoaded = false;

	async function getState() {
		try {
			const response = await fetch('/rest/effectsState', {
				method: 'GET',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			itemState = await response.json();
			console.log("itemState", itemState);
			if (itemState.nodes) //sometimes error null...
				itemsList = itemState.nodes;
			dataLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}

		try {
			const response = await fetch('/rest/starState', {
				method: 'GET',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			starState = await response.json();
			console.log("starState", starState);
			starLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}
		return itemState;
	}

	async function postSettings(data: EffectsState) {
		try {
			const response = await fetch('/rest/effectsState', {
				method: 'POST',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(data)
			});
			if (response.status == 200) {
				notifications.success('Settings updated.', 3000);
				itemState = await response.json();
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	function validateItem() {
		if (itemState.name.length < 3 || itemState.name.length > 32) {
			formErrorFilename = true;
		} else {
			formErrorFilename = false;
			// Update global itemState object
			itemState.nodes = itemsList;
			// Post to REST API
			postSettings(itemState);
			console.log(itemState);
		}
	}

	function validateForm() {
		let valid = true;

		// Validate Name
		if (editableItem.name.length < 3 || editableItem.name.length > 32) {
			valid = false;
			formErrors.name = true;
		} else {
			formErrors.name = false;
		}

		// Submit JSON to REST API
		if (valid) {
			if (newItem) {
				itemsList.push(editableItem);
			} else {
				itemsList.splice(itemsList.indexOf(editableItem), 1, editableItem);
			}
			addItem();
			itemsList = [...itemsList]; //Trigger reactivity
			showEditor = false;
		}
	}

	function addItem() {
		newItem = true;
		editableItem = {
			name: '',
			effect: -1,
			projection: -1,
			nodes: []
		};
	}

	function handleEdit(index: number) {
		newItem = false;
		showEditor = true;
		editableItem = itemsList[index];
	}

	function confirmDelete(index: number) {
		openModal(ConfirmDialog, {
			title: 'Delete item',
			message: 'Are you sure you want to delete ' + itemsList[index].name + '?',
			labels: {
				cancel: { label: 'Cancel', icon: Cancel },
				confirm: { label: 'Delete', icon: Delete }
			},
			onConfirm: () => {
				// Check if item is currently been edited and delete as well
				if (itemsList[index].name === editableItem.name) {
					addItem();
				}
				// Remove item from array
				itemsList.splice(index, 1);
				itemsList = [...itemsList]; //Trigger reactivity
				showEditor = false;
				closeModal();
			}
		});
	}

	function checkItemList() {
		if (itemsList.length >= 10) {
			openModal(InfoDialog, {
				title: 'Reached Maximum items',
				message:
					'You have reached the maximum number of items. Please delete one to add another.',
				dismiss: { label: 'OK', icon: Check },
				onDismiss: () => {
					closeModal();
				}
			});
			return false;
		} else {
			return true;
		}
	}

	function onDrop({ detail: { from, to } }: CustomEvent<DropEvent>) {
		if (!to || from === to) {
			return;
		}

		itemsList = reorder(itemsList, from.index, to.index);
		console.log(itemsList);
	}

	onMount(() => {
		socket.on<EffectsState>('effects', (data) => {
			itemState = data;
			dataLoaded = true;
		});
		socket.on<StarState>('stars', (data) => {
			console.log("star", data);
			starState = data;
			starLoaded = true;
		});
		// getState(); //done in settingscard
	});

	onDestroy(() => {
		socket.off("effects");
		socket.off("stars");
	});

	function sendSocket() {
		console.log("sendSocket", itemState);
		if (dataLoaded) 
			socket.sendEvent('effects', itemState)
	}

</script>

<SettingsCard collapsible={false}>
	<EffectsIcon slot="icon" class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	<span slot="title">Effects</span>
	<div class="w-full overflow-x-auto">
	</div>

	{#if !$page.data.features.security || $user.admin}
		<div class="bg-base-200 shadow-lg relative grid w-full max-w-2xl self-center overflow-hidden">
			{#await getState()}
				<Spinner />
			{:then nothing}
				<div>
					<Select label="Effect" bind:value={itemState.effect} onChange={sendSocket}>
						{#each starState.effects as effect, i}
							<option value={i}>
								{effect}
							</option>
						{/each}
					</Select>
					<Select label="Projection" bind:value={itemState.projection} onChange={sendSocket}>
						{#each starState.projections as projection, i}
							<option value={i}>
								{projection}
							</option>
						{/each}
					</Select>
				</div>

				<div class="h-16 flex w-full items-center justify-between space-x-3 p-0 text-xl font-medium">
					Effects and Projections
				</div>
				<div class="relative w-full overflow-visible">
					<button
						class="btn btn-primary text-primary-content btn-md absolute -top-14 right-16"
						on:click={() => {
							if (checkItemList()) {
								addItem();
								showEditor = true;
							}
						}}
					>
						<Add class="h-6 w-6" /></button
					>

					<div
						class="overflow-x-auto space-y-1"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<DragDropList
							id="effects"
							type={VerticalDropZone}
							itemSize={60}
							itemCount={itemsList.length}
							on:drop={onDrop}
							let:index
						>
							<!-- svelte-ignore a11y-click-events-have-key-events -->
							<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
									<EffectsIcon class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">{itemsList[index].name}</div>
								</div>
								{#if !$page.data.features.security || $user.admin}
									<div class="flex-grow" />
									<div class="space-x-0 px-0 mx-0">
										<button
											class="btn btn-ghost btn-sm"
											on:click={() => {
												handleEdit(index);
											}}
										>
											<Edit class="h-6 w-6" /></button
										>
										<button
											class="btn btn-ghost btn-sm"
											on:click={() => {
												confirmDelete(index);
											}}
										>
											<Delete class="text-error h-6 w-6" />
										</button>
									</div>
								{/if}
							</div>
						</DragDropList>
					</div>
				</div>

				<div
					class="flex flex-col gap-2 p-0"
					transition:slide|local={{ duration: 300, easing: cubicOut }}
				>
					<form
						class=""
						on:submit|preventDefault={validateForm}
						novalidate
						bind:this={formField}
					>

						{#if showEditor}
							<div class="divider my-0" />
							<div class="h-16 flex w-full items-center justify-between space-x-3 p-0 text-xl font-medium">
								Edit
							</div>
							<div
								class="grid w-full grid-cols-1 content-center gap-x-4 px-4 sm:grid-cols-2"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<div>
									<label class="label" for="name">
										<span class="label-text text-md">Name</span>
									</label>
									<input
										type="text"
										class="input input-bordered invalid:border-error w-full invalid:border-2 {formErrors.name
											? 'border-error border-2'
											: ''}"
										bind:value={editableItem.name}
										id="name"
										min="2"
										max="32"
										required
									/>
									<label class="label" for="name">
										<span class="label-text-alt text-error {formErrors.name ? '' : 'hidden'}"
											>Name must be between 3 and 32 characters long</span
										>
									</label>
								</div>
								<div>
									<Select label="Source" bind:value={editableItem.effect} onChange={()=>{}}>
										{#each starState.projections as projection, i}
											<option value={i}>
												{projection}
											</option>
										{/each}
									</Select>
								</div>
							</div>
						{/if}

						<div class="divider mb-2 mt-0" />
						<div class="mx-4 mb-4 flex flex-wrap justify-end gap-2">
							<button class="btn btn-primary" type="submit" disabled={!showEditor}
								>{newItem ? 'Add' : 'Update'}</button
							>
							<button class="btn btn-primary" type="button" on:click={validateItem}
								>Apply</button
							>
						</div>
					</form>
				</div>
			{/await}
		</div>
	{/if}
</SettingsCard>
