create table if not exists public.private_progress (
  user_id uuid primary key references auth.users (id) on delete cascade,
  salt text not null,
  iv text not null,
  ciphertext text not null,
  revision bigint not null default 1 check (revision > 0),
  updated_at timestamptz not null default now()
);

create or replace function public.set_private_progress_updated_at()
returns trigger
language plpgsql
set search_path = ''
as $$
begin
  new.updated_at = now();
  return new;
end;
$$;

drop trigger if exists set_private_progress_updated_at on public.private_progress;
create trigger set_private_progress_updated_at
  before update on public.private_progress
  for each row execute function public.set_private_progress_updated_at();

alter table public.private_progress enable row level security;
revoke all on table public.private_progress from anon, authenticated;
grant select, insert, update, delete on table public.private_progress to authenticated;

drop policy if exists "Users can read their own encrypted progress" on public.private_progress;
create policy "Users can read their own encrypted progress"
  on public.private_progress
  for select
  to authenticated
  using ((select auth.uid()) = user_id);

drop policy if exists "Users can create their own encrypted progress" on public.private_progress;
create policy "Users can create their own encrypted progress"
  on public.private_progress
  for insert
  to authenticated
  with check ((select auth.uid()) = user_id);

drop policy if exists "Users can update their own encrypted progress" on public.private_progress;
create policy "Users can update their own encrypted progress"
  on public.private_progress
  for update
  to authenticated
  using ((select auth.uid()) = user_id)
  with check ((select auth.uid()) = user_id);

drop policy if exists "Users can delete their own encrypted progress" on public.private_progress;
create policy "Users can delete their own encrypted progress"
  on public.private_progress
  for delete
  to authenticated
  using ((select auth.uid()) = user_id);
